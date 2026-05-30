# URL Shortener (C++)

A tiny **URL shortener** written in C++ with CLI, interactive mode, and a modern web UI.

It supports:
- generating a short code for a URL
- expanding a short code back to the original URL
- persisting mappings to `urls.db` in the current folder
- interactive prompt mode for quick typing
- web UI in browser (local or deployed)

## Deploy (Render + Docker)

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/NamanGupta05/URL_Shortener)

1. Click **Deploy to Render** (or open [Render Dashboard](https://dashboard.render.com/) → **New +** → **Blueprint**).
2. Connect GitHub and select repo: [NamanGupta05/URL_Shortener](https://github.com/NamanGupta05/URL_Shortener).
3. Render reads `render.yaml` and builds from `Dockerfile`.
4. After deploy, open your live URL (example: `https://url-shortener-xxxx.onrender.com`).

Notes:
- Render sets `PORT` and `RENDER_EXTERNAL_URL` automatically for short links.
- Free tier may sleep after inactivity; first request can take ~30 seconds.

## Build (Windows)

### Option A: CMake (recommended)

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run:

```bash
.\build\url_shortener.exe help
```

### Option B: g++ / clang++

```bash
g++ -std=c++14 -O2 -o url_shortener main.cpp
```

## Usage

Interactive mode (best experience):

```bash
.\build\url_shortener.exe
```

Then type any URL directly:

```text
> https://example.com/very/long/link
http://sho.rt/abc123
```

Useful commands in interactive mode:
- `expand <code|short-url>`
- `list`
- `help`
- `exit`

Web interface:

```bash
.\build\url_shortener.exe web 8080
```

Then open:
- `http://localhost:8080`

You can paste a long URL in the input box and click **Shorten URL**.

Shorten a URL:

```bash
.\url_shortener.exe shorten https://example.com/some/long/path
```

Expand a code (or the full short URL) back to the original:

```bash
.\url_shortener.exe expand abc123
```

```bash
.\url_shortener.exe expand http://sho.rt/abc123
```

List all mappings:

```bash
.\url_shortener.exe list
```

## Configure the base short domain

Set `SHORT_BASE` (must include scheme):

PowerShell:

```powershell
$env:SHORT_BASE = "https://my.short/"
.\url_shortener.exe shorten https://example.com
```

CMD:

```bat
set SHORT_BASE=https://my.short/
url_shortener.exe shorten https://example.com
```

## Docker (local)

```bash
docker build -t url-shortener .
docker run -p 8080:8080 -e PORT=8080 url-shortener
```

Open `http://localhost:8080`.

