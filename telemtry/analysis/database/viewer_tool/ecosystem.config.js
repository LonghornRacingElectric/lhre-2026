// pm2 process definition for the viewer tool.
//
// Managed by server_devtool.sh (`viewer` component): `build viewer` runs
// `npm run build` then `pm2 startOrReload` this file; `status`/`logs`/`stop`
// drive pm2 by the app name below. Keeping the run config here (instead of an
// ad-hoc `pm2 start` command) makes start/reload idempotent and reviewable.
//
// Run the Next.js binary DIRECTLY (not `npm run start`): with npm as the
// script, pm2 monitors the npm wrapper instead of the node server, so signals
// aren't forwarded cleanly (zombie node procs on stop/restart) and CPU/mem
// stats are wrong. Pointing at next's bin makes pm2 supervise the real process.
module.exports = {
  apps: [
    {
      name: 'viewer_tool',
      cwd: __dirname,
      script: 'node_modules/next/dist/bin/next',
      interpreter: 'node',
      args: 'start -p 3001',
      autorestart: true,
      max_restarts: 10,
      env: {
        // logsync runs host-networked on the same box; the proxy route also
        // defaults to this, so it's just being explicit.
        LOGSYNC_URL: 'http://localhost:8090',
      },
    },
  ],
};
