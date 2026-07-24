# Luma website (Cloudflare Pages)

Static marketing site for **Luma Builder**: product info, screenshots, and package downloads.

## Deploy on Cloudflare Pages

1. In [Cloudflare Dashboard](https://dash.cloudflare.com/) → **Workers & Pages** → **Create** → **Pages** → Connect the `Kkkppmm/Luma` GitHub repo.
2. Build settings:
   - **Framework preset:** None
   - **Build command:** *(leave empty)*
   - **Build output directory:** `website`
3. After the first deploy, open **Custom domains** and attach your Cloudflare domain (e.g. `luma.yourdomain.com` or apex).
4. Optional: Pages → Settings → Environment variables are not required for this static site.

### Deploy from CLI (optional)

```bash
npx wrangler pages deploy website --project-name=luma
```

Then add the custom domain in the Cloudflare Pages project settings.

## Local preview

```bash
python3 -m http.server 8080 --directory website
# open http://127.0.0.1:8080
```

## Updating downloads

Package links in `index.html` point at GitHub release assets for `v0.3.3`.
After a new release, bump the version strings in the Downloads section (or point them at `/releases/latest/download/...` if asset names stay stable).
