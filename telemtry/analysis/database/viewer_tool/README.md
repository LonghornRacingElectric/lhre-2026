This is a [Next.js](https://nextjs.org) project bootstrapped with [`create-next-app`](https://nextjs.org/docs/app/api-reference/cli/create-next-app).

## Getting Started

First, run the development server:

```bash
npm run dev
# or
yarn dev
# or
pnpm dev
# or
bun dev
```

Open [http://localhost:3000](http://localhost:3000) with your browser to see the result.

You can start editing the page by modifying `app/page.tsx`. The page auto-updates as you edit the file.

This project uses [`next/font`](https://nextjs.org/docs/app/building-your-application/optimizing/fonts) to automatically optimize and load [Geist](https://vercel.com/font), a new font family for Vercel.

## Kafka routing configuration

The viewer consumes raw Kafka topics and routes them to logical UI topics via a JSON config.

- Preferred: point to a JSON file with `KAFKA_ROUTES_FILE`.
- Backward-compatible: `KAFKA_ROUTES_JSON` also works (JSON string env var).
- Default: if neither is set, the app will try `./kafka.routes.json` from the project root of `viewer_tool`.

Example env:

```env
KAFKA_BROKERS=localhost:29092
KAFKA_CLIENT_ID=viewer-tool
KAFKA_GROUP_ID=viewer-tool-group
KAFKA_TOPICS=status
KAFKA_AUTO_CREATE=1
KAFKA_ROUTES_FILE=./kafka.routes.json
```

Example `kafka.routes.json`:

```json
{
	"status": [
		{ "to": "livebanner", "pick": ["battery", "odometer", "dynamics.speed"] }
	]
}
```

Notes:
- You can use dot-notation in `pick` to select nested fields (e.g. `dynamics.speed`).
- When picking nested paths, the output preserves structure, e.g. `{ "dynamics": { "speed": 123 } }`.
- If `rename` is also provided, it applies to the leaf key of each picked path (e.g. renaming `speed` to `v`).

## Learn More

To learn more about Next.js, take a look at the following resources:

- [Next.js Documentation](https://nextjs.org/docs) - learn about Next.js features and API.
- [Learn Next.js](https://nextjs.org/learn) - an interactive Next.js tutorial.

You can check out [the Next.js GitHub repository](https://github.com/vercel/next.js) - your feedback and contributions are welcome!

## Deploy on Vercel

The easiest way to deploy your Next.js app is to use the [Vercel Platform](https://vercel.com/new?utm_medium=default-template&filter=next.js&utm_source=create-next-app&utm_campaign=create-next-app-readme) from the creators of Next.js.

Check out our [Next.js deployment documentation](https://nextjs.org/docs/app/building-your-application/deploying) for more details.
