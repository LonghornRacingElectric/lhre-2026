#!/usr/bin/env bash
#
# Telemetry Server Devtool — the one tool for running the stack on the deploy box.
#
# Highlights:
#   * Detached-by-default: every "up" runs with -d, so closing the logs NEVER
#     kills the stack. Re-attach any time with `logs`; Ctrl-C just detaches.
#   * Disk-aware: the box has very little Docker headroom (~30 GB) and the build
#     cache silently fills it until the whole stack dies with no error. This tool
#     checks free space before every build, auto-trims the build cache when low,
#     and gives you prunes that NEVER touch the data volumes.
#   * No needless rebuilds: `up` reuses existing images (fast, no cache churn).
#     Rebuild only when you ask for it (`build`).
#   * Correct & idempotent: creates the external `telemetry_network` and the
#     external volumes if missing, so a fresh box just works. Uses compose
#     project labels for health/logs, so it never goes stale on renames.
#   * No sudo guessing: auto-detects whether docker needs sudo (root's
#     cli-plugins dir often lacks the compose plugin → bogus "v2 required").
#
# Usage:
#   ./server_devtool.sh                 interactive menu
#   ./server_devtool.sh <command> ...   run a command directly (scriptable)
#
# Commands:
#   status | ps        health of every component + disk usage
#   disk               docker disk usage + free space on the docker data dir
#   up                 start the core stack detached (reuses images, no rebuild)
#   build [comp...]    rebuild + start (core, or named components)
#   logs [comp...]     follow logs (Ctrl-C detaches; containers keep running)
#   attach             alias for `logs` on the core stack
#   restart [comp...]  restart component(s) (default: core)
#   stop               stop EVERYTHING (compose down; keeps volumes/data)
#   enable <comp...>   build + start optional processor(s)
#   prune              safe prune: build cache + dangling images (keeps volumes)
#   prune-deep         remove ALL unused images too (still keeps volumes)
#   reset-db           recreate telemetry_db volume  (DESTROYS telemetry data)
#   reset-all          recreate telemetry_db + grafana (DESTROYS dashboards too)
#   help               this text
#
# Core      : kafka ingest field_enricher
# Optional  : gps_classifier lap_timer track_mapper kafka_test gg_plot

set -uo pipefail

# ----------------------------------------------------------------------------- config
MIN_FREE_GB="${MIN_FREE_GB:-10}"   # warn / auto-trim build cache below this
CRIT_FREE_GB="${CRIT_FREE_GB:-5}"  # hard-warn + prompt below this

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_SOURCE_DIR="$SCRIPT_DIR/../analysis/database/dashboards"

NETWORK="telemetry_network"        # external in every compose file → we own it
EXTERNAL_VOLUMES="telemetry_db grafana_storage"

# component registry:  name | directory (relative to SCRIPT_DIR)
# (gg_plot is optional; kafka_base is a base image and grafana-kafka-datasource
#  is a plugin build helper — neither is a runtime service, so both are omitted.)
STACK_COMPONENTS="
kafka|kafka
ingest|ingest
field_enricher|processors/field_enricher
gps_classifier|processors/gps_classifier
lap_timer|processors/lap_timer
track_mapper|processors/track_mapper
kafka_test|processors/kafka_test
gg_plot|processors/gg_plot
"

# kafka broker first, then ingest (mosquitto/db/grafana), then the derived-field enricher
CORE_ORDER="kafka ingest field_enricher"
ALL_ORDER="kafka ingest field_enricher gps_classifier lap_timer track_mapper kafka_test gg_plot"

# ----------------------------------------------------------------------------- colors
if [[ -t 1 ]]; then
    C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_DIM=$'\033[2m'
    C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_CYN=$'\033[36m'
else
    C_RESET=""; C_BOLD=""; C_DIM=""; C_RED=""; C_GRN=""; C_YLW=""; C_CYN=""
fi
say()  { printf '%s\n' "$*"; }
info() { printf '%s==>%s %s\n' "$C_CYN$C_BOLD" "$C_RESET" "$*"; }
ok()   { printf '%s✓%s %s\n'  "$C_GRN" "$C_RESET" "$*"; }
warn() { printf '%s⚠ %s%s\n'  "$C_YLW" "$*" "$C_RESET"; }
err()  { printf '%s✗ %s%s\n'  "$C_RED" "$*" "$C_RESET" >&2; }

# ----------------------------------------------------------------------------- docker plumbing
OS="$(uname)"
SUDO=""   # auto-detected in require_docker: only use sudo if docker needs it

require_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        err "docker is not installed / not on PATH."; exit 1
    fi
    # Prefer running without sudo (user in the docker group). Only fall back to
    # sudo if a plain docker call fails — `sudo docker` uses root's cli-plugins
    # dir and often can't find the compose plugin, which is the usual cause of
    # the bogus "'docker compose' (v2) is required" error.
    if docker compose version >/dev/null 2>&1; then
        SUDO=""
    elif command -v sudo >/dev/null 2>&1 && sudo docker compose version >/dev/null 2>&1; then
        SUDO="sudo"
    elif docker version >/dev/null 2>&1 || { command -v sudo >/dev/null 2>&1 && sudo docker version >/dev/null 2>&1; }; then
        err "docker is reachable but the compose v2 plugin is missing. Install docker-compose-plugin."
        exit 1
    else
        err "cannot talk to the docker daemon (is it running? are you in the 'docker' group?)."
        exit 1
    fi
}
dk() { $SUDO docker "$@"; }

compose_in() {  # compose_in <dir> <args...>
    local dir="$1"; shift
    if [[ ! -f "$dir/docker-compose.yml" && ! -f "$dir/docker-compose.yaml" ]]; then
        warn "no docker-compose.(yml|yaml) in $dir — skipping"; return 1
    fi
    ( cd "$dir" && $SUDO docker compose "$@" )
}

# registry lookups -----------------------------------------------------------
comp_reldir() {  # absolute dir for a component name
    local name="$1" line
    while IFS= read -r line; do
        [[ -z "$line" ]] && continue
        if [[ "${line%%|*}" == "$name" ]]; then
            printf '%s/%s' "$SCRIPT_DIR" "${line#*|}"; return 0
        fi
    done <<< "$STACK_COMPONENTS"
    return 1
}
# compose's default project name is the lowercased dir basename with [^a-z0-9_-] stripped
comp_project() {
    local dir; dir="$(comp_reldir "$1")" || return 1
    basename "$dir" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9_-'
}

# ----------------------------------------------------------------------------- disk safety
docker_root() { dk info -f '{{.DockerRootDir}}' 2>/dev/null || echo /var/lib/docker; }
free_gb() {
    local root; root="$(docker_root)"; [[ -d "$root" ]] || root="/"
    df -BG --output=avail "$root" 2>/dev/null | tail -1 | tr -dc '0-9' || echo 0
}

disk_summary() {
    local root free; root="$(docker_root)"; free="$(free_gb)"
    info "Docker disk usage  (root: $root)"
    dk system df 2>/dev/null || true
    echo
    if (( ${free:-0} < CRIT_FREE_GB )); then
        err "Only ${free} GB free — CRITICAL. The stack can fail silently. Run: $0 prune-deep"
    elif (( ${free:-0} < MIN_FREE_GB )); then
        warn "Only ${free} GB free (warn threshold ${MIN_FREE_GB} GB). Consider: $0 prune"
    else
        ok "${free} GB free on $root"
    fi
}

# Run before any build. Auto-trims build cache when low; prompts if critical.
preflight_disk() {
    local free; free="$(free_gb)"
    (( ${free:-0} >= MIN_FREE_GB )) && return 0
    warn "Low disk before build: ${free} GB free (< ${MIN_FREE_GB} GB)."
    info "Auto-trimming docker build cache + dangling images..."
    dk builder prune -f >/dev/null 2>&1 || true
    dk image prune -f   >/dev/null 2>&1 || true
    free="$(free_gb)"; ok "After trim: ${free} GB free"
    if (( ${free:-0} < CRIT_FREE_GB )); then
        err "Still only ${free} GB free."
        if confirm "Run a deep prune (remove ALL unused images, keep volumes) now?"; then
            prune_deep; free="$(free_gb)"
        fi
    fi
    (( ${free:-0} < CRIT_FREE_GB )) && warn "Disk still critical (${free} GB). Build may fail; continuing."
    return 0
}

prune_safe() {
    info "Safe prune: build cache + dangling images (volumes untouched)"
    dk builder prune -f || true
    dk image prune -f   || true
    ok "Done."; disk_summary
}
prune_deep() {
    info "Deep prune: ALL unused images + build cache (data volumes are protected)"
    dk system prune -af || true   # deliberately NOT --volumes
    ok "Done."; disk_summary
}

# ----------------------------------------------------------------------------- preconditions
ensure_network() {
    dk network inspect "$NETWORK" >/dev/null 2>&1 && return 0
    info "Creating missing external network: $NETWORK"
    dk network create "$NETWORK" >/dev/null
}
ensure_volumes() {
    local v
    for v in $EXTERNAL_VOLUMES; do
        if ! dk volume inspect "$v" >/dev/null 2>&1; then
            info "Creating missing external volume: $v"
            dk volume create "$v" >/dev/null
        fi
    done
}
ensure_dashboards() {
    [[ -d "$DASHBOARD_SOURCE_DIR" ]] || \
        warn "Dashboard source not found: $DASHBOARD_SOURCE_DIR (grafana may start empty)"
}
# ingest publishes 5432; a host postgres steals that port. Only relevant before ingest.
free_db_port() {
    if [[ "$OS" == "Linux" ]]; then
        if id postgres >/dev/null 2>&1 && pgrep -u postgres >/dev/null 2>&1; then
            info "Stopping host postgres to free port 5432..."
            $SUDO pkill -u postgres 2>/dev/null || true
        fi
    else
        command -v brew >/dev/null 2>&1 && brew services stop postgresql >/dev/null 2>&1 || true
    fi
}

# ----------------------------------------------------------------------------- lifecycle
up_component() {  # up_component <name> <build:1|0>
    local name="$1" build="${2:-0}" dir
    dir="$(comp_reldir "$name")" || { err "unknown component: $name"; return 1; }
    ensure_network
    if [[ "$name" == "ingest" ]]; then
        ensure_volumes; ensure_dashboards; free_db_port
    fi
    if [[ "$build" == "1" ]]; then
        preflight_disk
        info "Building + starting $name ..."
        compose_in "$dir" up --build -d
    else
        info "Starting $name ..."
        compose_in "$dir" up -d
    fi
}
down_component() {
    local name="$1" dir; dir="$(comp_reldir "$name")" || return 1
    info "Stopping $name ..."
    compose_in "$dir" down
}
up_set() {  # up_set <build:1|0> <name...>
    local build="$1"; shift; local n
    for n in "$@"; do up_component "$n" "$build" || return 1; done
}
stop_all() {
    local rev="" n
    for n in $ALL_ORDER; do rev="$n $rev"; done   # reverse order
    for n in $rev; do
        local dir; dir="$(comp_reldir "$n")"
        [[ -f "$dir/docker-compose.yml" || -f "$dir/docker-compose.yaml" ]] && down_component "$n"
    done
    ok "All telemetry stack services stopped (volumes/data kept)."
}
restart_set() {
    local n
    for n in "$@"; do
        local dir; dir="$(comp_reldir "$n")" || continue
        info "Restarting $n ..."
        compose_in "$dir" restart 2>/dev/null || up_component "$n" 0
    done
}

logs_for() {  # logs_for <name...>  (default core); Ctrl-C detaches
    local names=("$@")
    [[ ${#names[@]} -eq 0 ]] && names=($CORE_ORDER)
    warn "Following logs — press Ctrl-C to DETACH (containers keep running)."; echo
    if [[ ${#names[@]} -eq 1 ]]; then
        compose_in "$(comp_reldir "${names[0]}")" logs -f --tail 100
        return
    fi
    # multiple compose projects → fan out, prefix each. On Ctrl-C kill ONLY our
    # own child tails (never `kill 0`, which could signal the parent shell).
    local pids=() n dir
    # shellcheck disable=SC2064
    trap 'kill "${pids[@]}" 2>/dev/null; trap - INT TERM' INT TERM
    for n in "${names[@]}"; do
        dir="$(comp_reldir "$n")" || continue
        compose_in "$dir" logs -f --tail 50 2>&1 | sed "s/^/${C_DIM}[$n]${C_RESET} /" &
        pids+=($!)
    done
    wait
    trap - INT TERM
}

# ----------------------------------------------------------------------------- status / health
status() {
    require_docker
    info "Telemetry stack health"; echo
    printf '%s%-16s %-26s %-12s %s%s\n' "$C_BOLD" "COMPONENT" "CONTAINER" "STATE" "STATUS" "$C_RESET"
    local degraded=0 name proj rows cname state st color rc
    for name in $ALL_ORDER; do
        proj="$(comp_project "$name")"
        rows="$(dk ps -a --filter "label=com.docker.compose.project=$proj" \
                  --format '{{.Names}}|{{.State}}|{{.Status}}' 2>/dev/null)"
        if [[ -z "$rows" ]]; then
            printf '%-16s %-26s %s%-12s%s\n' "$name" "-" "$C_DIM" "not created" "$C_RESET"
            continue
        fi
        local first=1
        while IFS='|' read -r cname state st; do
            [[ -z "$cname" ]] && continue
            case "$state" in
                running)
                    if [[ "$st" == *unhealthy* ]]; then color="$C_YLW"; degraded=1; else color="$C_GRN"; fi ;;
                restarting)
                    color="$C_RED"; degraded=1
                    rc="$(dk inspect -f '{{.RestartCount}}' "$cname" 2>/dev/null || echo '?')"
                    st="$st (crash-loop, restarts=$rc)" ;;
                exited|dead) color="$C_RED"; degraded=1 ;;
                *)           color="$C_YLW" ;;
            esac
            local label=""; [[ $first == 1 ]] && label="$name"; first=0
            printf '%-16s %-26s %s%-12s %s%s\n' "$label" "$cname" "$color" "$state" "$st" "$C_RESET"
        done <<< "$rows"
    done
    echo
    if dk network inspect "$NETWORK" >/dev/null 2>&1; then ok "network '$NETWORK' present"
    else warn "network '$NETWORK' MISSING (created automatically on next start)"; fi
    local v
    for v in $EXTERNAL_VOLUMES; do
        if dk volume inspect "$v" >/dev/null 2>&1; then ok "volume '$v' present"
        else warn "volume '$v' MISSING (created automatically on next start)"; fi
    done
    echo
    if (( degraded )); then
        err "Something is unhealthy / crash-looping. If disk is full this is the cause — see below."
    else
        ok "All present containers healthy."
    fi
    echo; disk_summary
}

# ----------------------------------------------------------------------------- helpers
confirm() {
    local q="$1" ans
    printf '%s%s [y/N]%s ' "$C_YLW" "$q" "$C_RESET"; read -r ans
    [[ "$ans" =~ ^[Yy]$ ]]
}
reset_volume() {
    local v="$1"
    dk volume rm "$v" >/dev/null 2>&1 || true
    dk volume create "$v" >/dev/null
    ok "recreated volume $v"
}
cmd_reset_db() {
    require_docker
    warn "This DESTROYS all telemetry timeseries data (telemetry_db)."
    confirm "Proceed?" || { say "Aborted."; return 0; }
    down_component ingest
    reset_volume telemetry_db
    up_set 1 $CORE_ORDER; status
}
cmd_reset_all() {
    require_docker
    err "This DESTROYS telemetry data AND all Grafana dashboards/settings."
    confirm "Are you absolutely sure?" || { say "Crisis averted!"; return 0; }
    down_component ingest
    reset_volume telemetry_db
    reset_volume grafana_storage
    up_set 1 $CORE_ORDER; status
}
usage() { sed -n '3,/^set -uo pipefail/p' "${BASH_SOURCE[0]}" | sed 's/^#\{1,\} \{0,1\}//; /^set -uo pipefail/d'; }

# ----------------------------------------------------------------------------- command dispatch
run_command() {
    local cmd="$1"; shift || true
    case "$cmd" in
        status|ps)   status ;;
        disk|df)     require_docker; disk_summary ;;
        up|start)    require_docker; up_set 0 $CORE_ORDER; echo; ok "Core stack up (detached). Logs: $0 logs" ;;
        build|rebuild)
            require_docker
            if [[ $# -gt 0 ]]; then up_set 1 "$@"; else up_set 1 $CORE_ORDER; fi
            echo; ok "Built + started (detached). Logs: $0 logs" ;;
        logs|attach) require_docker; logs_for "$@" ;;
        restart)     require_docker; if [[ $# -gt 0 ]]; then restart_set "$@"; else restart_set $CORE_ORDER; fi ;;
        stop|down)   require_docker; stop_all ;;
        enable)      require_docker; [[ $# -gt 0 ]] || { err "usage: $0 enable <component...>"; return 1; }; up_set 1 "$@" ;;
        prune)       require_docker; prune_safe ;;
        prune-deep)  require_docker; prune_deep ;;
        reset-db)    cmd_reset_db ;;
        reset-all)   cmd_reset_all ;;
        help|-h|--help) usage ;;
        *)           err "unknown command: $cmd"; echo; usage; return 1 ;;
    esac
}

# ----------------------------------------------------------------------------- interactive menu
menu() {
    require_docker
    while :; do
        echo
        printf '%s===== Telemetry Server Devtool =====%s\n' "$C_BOLD$C_CYN" "$C_RESET"
        printf '  %sRun & inspect%s\n' "$C_BOLD" "$C_RESET"
        echo  "   1) Status / health + disk"
        echo  "   2) Start core stack          (detached, no rebuild — fast)"
        echo  "   3) Rebuild + start core      (disk-checked build)"
        echo  "   4) Follow logs / attach      (Ctrl-C detaches, keeps running)"
        echo  "   5) Restart core"
        echo  "   6) Stop EVERYTHING           (keeps data)"
        printf '  %sOptional processors%s\n' "$C_BOLD" "$C_RESET"
        echo  "   7) Enable gps_classifier        8) Enable lap_timer"
        echo  "   9) Enable track_mapper          0) Enable kafka_test"
        echo  "   m) Enable gg_plot"
        printf '  %sDisk%s\n' "$C_BOLD" "$C_RESET"
        echo  "   p) Prune (safe: cache + dangling)   P) Prune DEEP (all unused images)"
        printf '  %sDanger zone%s\n' "$C_BOLD" "$C_RESET"
        echo  "   d) Reset telemetry_db (DESTROYS data)   D) Reset db + grafana (DESTROYS dashboards)"
        echo
        echo  "   q) Quit (leaves the stack running)"
        printf '%sSelect: %s' "$C_BOLD" "$C_RESET"
        local opt; read -r opt; echo
        case "$opt" in
            1) status ;;
            2) up_set 0 $CORE_ORDER; ok "Core stack up (detached)." ;;
            3) up_set 1 $CORE_ORDER; ok "Rebuilt + started (detached)." ;;
            4) logs_for $CORE_ORDER ;;
            5) restart_set $CORE_ORDER ;;
            6) stop_all ;;
            7) up_set 1 gps_classifier ;;
            8) up_set 1 lap_timer ;;
            9) up_set 1 track_mapper ;;
            0) up_set 1 kafka_test ;;
            m|M) up_set 1 gg_plot ;;
            p) prune_safe ;;
            P) prune_deep ;;
            d) cmd_reset_db ;;
            D) cmd_reset_all ;;
            q|Q) say "Bye — stack left running. Re-attach with: $0 logs"; break ;;
            "") ;;
            *) warn "Invalid selection." ;;
        esac
    done
}

# ----------------------------------------------------------------------------- entrypoint
if [[ $# -gt 0 ]]; then
    require_docker
    run_command "$@"
else
    menu
fi
