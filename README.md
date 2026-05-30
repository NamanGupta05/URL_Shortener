# URL Shortener

[![Live Demo](https://img.shields.io/badge/Live%20Demo-Open%20App-blue?style=for-the-badge)](https://url-shortener-6a0z.onrender.com/)

A simple URL shortener built with **C++**. Paste a long link and get a short one that redirects to the original page.

**Live app:** [https://url-shortener-6a0z.onrender.com/](https://url-shortener-6a0z.onrender.com/)

---

## How to use (online)

No installation needed. Open the live app and follow these steps:

1. Go to **[https://url-shortener-6a0z.onrender.com/](https://url-shortener-6a0z.onrender.com/)**
2. Paste your long URL in the input box (must start with `http://` or `https://`)
3. Click **Shorten URL**
4. Copy the short link shown on the page
5. Open the short link in a new tab — it will redirect to your original URL

### Example

| Step | Value |
|------|-------|
| Original URL | `https://example.com/very/long/path/to/page` |
| Short URL | `https://url-shortener-6a0z.onrender.com/abc123` |

When someone opens the short URL, they are sent to the original pages automatically.

> **Note:** On the free Render plan, the app may sleep when unused. The first visit after idle time can take about 30 seconds to load.

---

## Features

- Shorten long URLs instantly from the browser
- Click short links to redirect to the original URL
- Modern dark-themed web UI
- CLI and interactive terminal mode for local use
- Saves URL mappings locally in `urls.db`

---

## Run locally (Windows)

### 1. Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### 2. Start web UI

```powershell
.\build\url_shortener.exe web 8080
```

Open [http://localhost:8080](http://localhost:8080)

### 3. Interactive mode (terminal)

```powershell
.\build\url_shortener.exe
```

Type a URL directly:

```text
> https://example.com/very/long/link
http://localhost:8080/abc123
```

**Commands in interactive mode:**

| Command | Description |
|---------|-------------|
| `<url>` | Shorten a URL |
| `expand <code>` | Get original URL from short code |
| `list` | Show all saved mappings |
| `help` | Show command help |
| `exit` | Quit |

---

## CLI commands

```powershell
# Shorten a URL
.\build\url_shortener.exe shorten https://example.com/page

# Expand a short code
.\build\url_shortener.exe expand abc123

# List all mappings
.\build\url_shortener.exe list
```

---

## Deploy your own copy

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/NamanGupta05/URL_Shortener)

1. Click **Deploy to Render**
2. Connect your GitHub account
3. Select this repository
4. Wait for the build to finish
5. Open your new live URL

Or run with Docker locally:

```bash
docker build -t url-shortener .
docker run -p 8080:8080 -e PORT=8080 url-shortener
```

---

## Project structure

| File | Purpose |
|------|---------|
| `main.cpp` | Core logic, web server, and CLI |
| `CMakeLists.txt` | Build configuration |
| `Dockerfile` | Container setup for deployment |
| `render.yaml` | Render deployment config |
| `urls.db` | Local storage for URL mappings (created at runtime) |

---

## Tech stack

- C++14
- CMake
- Custom HTTP server (WinSock on Windows, BSD sockets on Linux)
- Docker + Render for deployment
