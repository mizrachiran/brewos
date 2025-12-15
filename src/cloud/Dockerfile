# Build stage
FROM node:20-alpine AS builder
WORKDIR /app

COPY package*.json ./
RUN npm ci

COPY tsconfig.json ./
COPY src ./src
RUN npm run build

# Production stage
FROM node:20-alpine
WORKDIR /app

RUN mkdir -p /data

COPY --from=builder /app/dist ./dist
COPY --from=builder /app/package*.json ./
RUN npm ci --production

# Copy pre-built web UI (built on host, copied to ./web before docker build)
COPY web ./web

# Copy pre-built admin UI (built on host in ./admin/dist)
COPY admin/dist ./admin/dist

ENV NODE_ENV=production
ENV WEB_DIST_PATH=/app/web
ENV ADMIN_DIST_PATH=/app/admin/dist
EXPOSE 3001

CMD ["node", "dist/server.js"]

