@echo off
REM Proxy: stdio → HTTP to doc-rag server
REM Используется в mcp.json вместо HTTP-транспорта
npx -y supergateway --sse http://doc-mcp.misc-server:3333/mcp
