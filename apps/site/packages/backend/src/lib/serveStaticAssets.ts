import { createHash } from "node:crypto";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { isAbsolute, relative, resolve } from "node:path";
import { invariant, pages } from "@arenacraft/common";
import type { Context } from "hono";
import { createMiddleware } from "hono/factory";

const isProduction = () => process.env["NODE_ENV"] === "production";

export const serveStaticAssets = (absoluteStaticAssetsPath: string) => {
  invariant(
    isAbsolute(absoluteStaticAssetsPath),
    "serveStaticAssets: absoluteStaticAssetsPath must be an absolute path",
  );
  const pageEntries = Object.entries(pages);
  const indexHtmlPath = resolve(absoluteStaticAssetsPath, pages["/"]);
  const isBuilt = existsSync(indexHtmlPath);

  if (!isBuilt) {
    const pageRoutes = new Set(pageEntries.map(([route]) => normalizeRoutePath(route)));

    return createMiddleware(async (ctx, next) => {
      if (
        (ctx.req.method === "GET" || ctx.req.method === "HEAD") &&
        pageRoutes.has(normalizeRoutePath(ctx.req.path))
      ) {
        return ctx.text("Missing frontend assets", 404);
      }
      return next();
    });
  }

  const assetLookupMap = new Map<string, string>();

  for (const [route, htmlFileName] of pageEntries) {
    const normalizedRoute = normalizeRoutePath(route);
    const htmlFilePath = resolve(absoluteStaticAssetsPath, htmlFileName);

    assetLookupMap.set(normalizedRoute, htmlFilePath);

    if (normalizedRoute !== "/") {
      assetLookupMap.set(`${normalizedRoute}/`, htmlFilePath);
    }
  }

  collectStaticAssets(absoluteStaticAssetsPath, assetLookupMap);

  const pagePrefixEntries = pageEntries
    .filter(([route]) => route !== "/")
    .map(
      ([route, htmlFileName]) => [route, resolve(absoluteStaticAssetsPath, htmlFileName)] as const,
    );

  const pageCache = new Map<string, CachedFile>();
  for (const [, htmlFileName] of pageEntries) {
    const htmlFilePath = resolve(absoluteStaticAssetsPath, htmlFileName);
    const cached = loadHtmlFile(htmlFilePath);
    if (cached) pageCache.set(htmlFilePath, cached);
  }

  return createMiddleware(async (ctx, next) => {
    if (ctx.req.path.startsWith("/api")) return next();
    if (ctx.req.method !== "GET" && ctx.req.method !== "HEAD") return next();

    if (isProduction() && ctx.req.path.endsWith(".map")) {
      return ctx.text("Not Found", 404);
    }

    const filePath =
      assetLookupMap.get(ctx.req.path) ?? assetLookupMap.get(normalizeRoutePath(ctx.req.path));

    if (filePath) {
      return serveFile(ctx, filePath, absoluteStaticAssetsPath, pageCache.get(filePath));
    }

    for (const [prefix, htmlPath] of pagePrefixEntries) {
      if (ctx.req.path.startsWith(`${prefix}/`)) {
        return serveFile(ctx, htmlPath, absoluteStaticAssetsPath, pageCache.get(htmlPath));
      }
    }

    if (
      ctx.req.path.endsWith(".js") ||
      ctx.req.path.endsWith(".css") ||
      ctx.req.path.endsWith(".map")
    ) {
      return ctx.text("Not Found", 404);
    }

    return serveFile(ctx, indexHtmlPath, absoluteStaticAssetsPath, pageCache.get(indexHtmlPath));
  });
};

function collectStaticAssets(
  absoluteStaticAssetsPath: string,
  assetLookupMap: Map<string, string> = new Map(),
  dirPath?: string,
) {
  for (const entry of readdirSync(dirPath ?? absoluteStaticAssetsPath, { withFileTypes: true })) {
    const fullPath = resolve(dirPath ?? absoluteStaticAssetsPath, entry.name);
    if (entry.isDirectory()) {
      collectStaticAssets(absoluteStaticAssetsPath, assetLookupMap, fullPath);
      continue;
    }
    const requestPath = `/${fullPath
      .slice(absoluteStaticAssetsPath.length)
      .replace(/\\/g, "/")
      .replace(/^\/+/, "")}`;
    assetLookupMap.set(requestPath, fullPath);
  }
}

type CachedFile = {
  content: Uint8Array;
  etag: string;
};

function loadHtmlFile(filePath: string): CachedFile | null {
  try {
    return createCachedFile(readFileSync(filePath));
  } catch {
    return null;
  }
}

function createCachedFile(content: Uint8Array): CachedFile {
  return {
    content,
    etag: `"${createHash("sha1").update(content).digest("base64url")}"`,
  };
}

const fileCache = new Map<string, CachedFile>();

async function serveFile(
  ctx: Context,
  filePath: string,
  root: string,
  preloaded?: CachedFile,
): Promise<Response> {
  if (!isUnderRoot(filePath, root)) {
    return new Response("Not Found", { status: 404 });
  }

  let cached = preloaded ?? fileCache.get(filePath);
  if (!cached) {
    let content: Uint8Array;
    try {
      content = await readFile(filePath);
    } catch {
      return new Response("Not Found", { status: 404 });
    }
    cached = createCachedFile(content);
    fileCache.set(filePath, cached);
  }

  const headers: Record<string, string> = {
    "Content-Type": getContentType(filePath),
    "Cache-Control": getCacheControl(filePath, root),
    ETag: cached.etag,
  };

  if (ctx.req.header("If-None-Match") === cached.etag) {
    return new Response(null, { status: 304, headers });
  }

  if (ctx.req.method === "HEAD") {
    return new Response(null, { status: 200, headers });
  }

  return new Response(cached.content, { headers });
}

function getCacheControl(filePath: string, root: string): string {
  if (filePath.endsWith(".html")) {
    return "no-cache";
  }
  const relativePath = relative(root, filePath).replace(/\\/g, "/");
  if (relativePath.startsWith("assets/")) {
    return "public, max-age=31536000, immutable";
  }
  return "public, max-age=3600";
}

const mimeTypes: Record<string, string> = {
  ".html": "text/html",
  ".js": "application/javascript",
  ".mjs": "application/javascript",
  ".css": "text/css",
  ".json": "application/json",
  ".png": "image/png",
  ".svg": "image/svg+xml",
  ".ico": "image/x-icon",
  ".woff2": "font/woff2",
  ".woff": "font/woff",
  ".map": "application/json",
};

function getContentType(filePath: string): string {
  const dotIdx = filePath.lastIndexOf(".");
  if (dotIdx === -1) return "application/octet-stream";
  return mimeTypes[filePath.slice(dotIdx)] ?? "application/octet-stream";
}

function normalizeRoutePath(path: string) {
  if (path === "/") return path;
  const normalizedPath = path.replace(/\/+$/, "");
  return normalizedPath === "" ? "/" : normalizedPath;
}

function isUnderRoot(filePath: string, root: string): boolean {
  const rel = relative(root, filePath);
  return !rel.startsWith("..") && !isAbsolute(rel);
}