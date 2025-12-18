FROM golang:1.21-alpine AS builder
RUN apk add --no-cache git build-base protobuf protobuf-dev
# Pin plugin versions compatible with Go 1.21
RUN go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.33.0
RUN go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.3.0

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download

# Copy proto files and generate Go code
COPY proto/ ./proto/
RUN mkdir -p proto/bridge proto/sensor && \
    protoc --go_out=. --go_opt=paths=source_relative \
    --go-grpc_out=. --go-grpc_opt=paths=source_relative \
    proto/bridge.proto && \
    protoc --go_out=. --go_opt=paths=source_relative \
    proto/sensor/sensor.proto

# Copy source and build
COPY cmd/ ./cmd/
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o /out/bridge ./cmd/bridge

FROM alpine:3.18
RUN apk add --no-cache ca-certificates
COPY --from=builder /out/bridge /usr/local/bin/bridge
ENV BRIDGE_ADDR=:50051
EXPOSE 50051
ENTRYPOINT ["/usr/local/bin/bridge"]