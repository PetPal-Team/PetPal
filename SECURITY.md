# Security Policy

## Supported Versions

PetPal is an actively developed homebrew project. Only the latest release
receives security updates — please update before reporting.

| Version        | Supported          |
| -------------- | ------------------ |
| 0.1.4 (latest) | :white_check_mark: |
| < 0.1.4        | :x:                |

## Reporting a Vulnerability

**Please do not** open a public GitHub issue for security problems, and don't
post them in the public Discord channels.

Report privately through either:

- **GitHub (preferred)** — go to the **Security** tab → **Report a
  vulnerability** to open a private advisory.
- **Discord** — DM a maintainer/admin in our server:
  https://discord.gg/6hwsE4WNDj

Please include:

- A description of the issue and its impact
- Steps to reproduce (a proof-of-concept if possible)
- The affected component (3DS app, Android app, `teampetpal.com` API, admin
  panel, or Discord bot) and version
- Any relevant logs/requests, or your Pet ID (`PP-XXXX-XXXX`) if it's
  account-related

### What to expect

- We aim to acknowledge reports within **72 hours**.
- We'll confirm the issue, keep you posted on progress, and tell you when a fix
  ships.
- Valid reports are credited in the release notes unless you'd rather stay
  anonymous.
- Please give us a reasonable chance to fix the issue before public disclosure.

## Scope

**In scope**

- The pass relay and account/moderation API at `teampetpal.com` (Cloudflare
  Pages Functions + Workers KV)
- The admin panel (`admin.teampetpal.com`), including TOTP-gated actions
- Account/token handling, ban/badge enforcement, and cross-device linking in the
  3DS and Android apps
- The Discord bot

**Out of scope**

- Denial-of-service / volumetric attacks
- Issues requiring a physical, already-compromised device beyond PetPal's own
  logic
- Bugs in third-party dependencies or the Cloudflare / Discord / 3DS / Android
  platforms themselves (report those upstream)
- Social engineering, spam, or self-inflicted ("self-XSS") issues
- Missing best-practice headers with no demonstrable impact

## Security model (context for researchers)

- The 3DS app stores **no site secrets** — each device mints a per-device token
  at runtime that lives only in the SD-card save, never in the binary.
- Public status checks (ban / version / profile badges) are **ID-only** and
  require no token.
- Privileged admin actions (bans, badge assignment) are gated behind a login
  session **and** a Google Authenticator (TOTP) code.

Thanks for helping keep PetPal and its players safe! 🐾
