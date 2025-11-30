# Cloud Service Deployment Guide

This guide covers deploying the BrewOS cloud service.

## Prerequisites

1. Build the web UI:
   ```bash
   cd src/web
   npm install
   npm run build
   ```

2. Build the cloud service:
   ```bash
   cd src/cloud
   npm install
   npm run build
   ```

## Local Development

```bash
cd src/cloud
npm run dev
```

Access at `http://localhost:3001`

## Environment Variables

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `PORT` | No | 3001 | HTTP/WebSocket port |
| `DATA_DIR` | No | `.` | Directory for SQLite database |
| `SUPABASE_JWT_SECRET` | Yes | - | Supabase JWT secret for auth |
| `CORS_ORIGIN` | No | `*` | Allowed CORS origins |
| `WEB_DIST_PATH` | No | `../web/dist` | Path to web UI build |

## Docker Deployment

### Dockerfile

Create `src/cloud/Dockerfile`:

```dockerfile
# Build stage
FROM node:20-alpine AS builder
WORKDIR /app

# Copy package files
COPY package*.json ./
RUN npm ci

# Copy source and build
COPY tsconfig.json ./
COPY src ./src
RUN npm run build

# Production stage
FROM node:20-alpine
WORKDIR /app

# Create data directory for SQLite
RUN mkdir -p /data

# Copy built files
COPY --from=builder /app/dist ./dist
COPY --from=builder /app/package*.json ./
RUN npm ci --production

# Copy web UI (pre-built)
COPY ../web/dist ./web

ENV NODE_ENV=production
ENV WEB_DIST_PATH=./web
ENV DATA_DIR=/data
EXPOSE 3001

CMD ["node", "dist/server.js"]
```

### Docker Compose

```yaml
version: '3.8'

services:
  brewos-cloud:
    build:
      context: .
      dockerfile: Dockerfile
    ports:
      - "3001:3001"
    volumes:
      - brewos-data:/data
    environment:
      - PORT=3001
      - DATA_DIR=/data
      - SUPABASE_JWT_SECRET=${SUPABASE_JWT_SECRET}
    restart: unless-stopped

volumes:
  brewos-data:
```

### Build and Run

```bash
docker build -t brewos-cloud .
docker run -p 3001:3001 \
  -v brewos-data:/data \
  -e SUPABASE_JWT_SECRET=your-secret \
  brewos-cloud
```

## SSL/TLS Configuration

### Using Reverse Proxy (Nginx)

```nginx
server {
    listen 443 ssl http2;
    server_name cloud.example.com;

    ssl_certificate /etc/letsencrypt/live/cloud.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/cloud.example.com/privkey.pem;

    location / {
        proxy_pass http://localhost:3001;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

### Using Caddy (Automatic HTTPS)

```
cloud.example.com {
    reverse_proxy localhost:3001
}
```

## Health Checks

The service exposes `/api/health` for health monitoring:

```bash
curl http://localhost:3001/api/health
```

Response:
```json
{
  "status": "ok",
  "timestamp": "2024-01-01T00:00:00.000Z",
  "devices": 5,
  "clients": 12
}
```

## Data Persistence

The SQLite database is stored in `DATA_DIR/brewos.db`. Ensure this directory is:
- Mounted as a volume in Docker
- Backed up regularly
- On persistent storage if using cloud platforms
