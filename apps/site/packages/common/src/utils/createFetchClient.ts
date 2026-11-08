import { GenericSchema, InferOutput, parse as vParse } from "valibot";

export type CreateFetchClientOptions<TContract extends FetchContract = FetchContract> = {
  baseUrl?: string;
  fetch?: typeof globalThis.fetch;
  overrides?: RouteOverrides<TContract>;
};

export function createFetchClient<TContract extends FetchContract>(
  contract: TContract,
  options: CreateFetchClientOptions<TContract> = {},
): FetchClient<TContract> {
  const fetchFn = options.fetch ?? globalThis.fetch;
  const baseUrl = options.baseUrl ?? "";

  const client: Partial<Record<keyof TContract, unknown>> = {};

  for (const key of Object.keys(contract) as Array<keyof TContract>) {
    const route = contract[key];
    const override = options.overrides?.[key] as RouteMethod<TContract[typeof key]> | undefined;

    client[key] = async (args?: ClientRequestArgs) => {
      const params = route.params ? vParse(route.params, args?.params) : undefined;
      const query = route.query ? vParse(route.query, args?.query) : undefined;
      const body = route.request ? vParse(route.request, args?.body) : undefined;

      if (override) {
        const result = await invokeOverride(override, buildRouteArgs(route, params, query, body));
        return vParse(route.response, result);
      }

      const path = interpolatePath(route.path, params as Record<string, unknown> | undefined);

      const searchParams = new URLSearchParams();
      if (query) {
        for (const [k, val] of Object.entries(query as Record<string, unknown>)) {
          if (val !== undefined && val !== null) {
            searchParams.set(k, String(val));
          }
        }
      }
      const queryString = searchParams.toString();
      const finalUrl = `${baseUrl}${path}${queryString ? "?" + queryString : ""}`;

      const response = await fetchFn(finalUrl, {
        method: route.method,
        ...(body === undefined
          ? {}
          : {
              headers: { "content-type": "application/json" },
              body: JSON.stringify(body),
            }),
      });

      await ensureOk(response);
      const responseBodyAsJson = await response.json();
      return vParse(route.response, responseBodyAsJson);
    };
  }

  return client as FetchClient<TContract>;
}

type Route = {
  method: string;
  path: string;
  response: GenericSchema;
  request?: GenericSchema;
  params?: GenericSchema;
  query?: GenericSchema;
};

type FetchContract = Record<string, Route>;

type RouteArgs<TRoute extends Route> = (TRoute extends {
  params: infer TParams extends GenericSchema;
}
  ? { params: InferOutput<TParams> }
  : {}) &
  (TRoute extends { query: infer TQuery extends GenericSchema }
    ? { query: InferOutput<TQuery> }
    : {}) &
  (TRoute extends { request: infer TRequest extends GenericSchema }
    ? { body: InferOutput<TRequest> }
    : {});

type FetchClient<TContract extends FetchContract> = {
  [TKey in keyof TContract]: RouteMethod<TContract[TKey]>;
};

type RouteOverrides<TContract extends FetchContract> = Partial<{
  [TKey in keyof TContract]: RouteMethod<TContract[TKey]>;
}>;

type ClientRequestArgs = {
  params?: unknown;
  query?: unknown;
  body?: unknown;
};

type RouteMethod<TRoute extends Route> = keyof RouteArgs<TRoute> extends never
  ? () => Promise<InferOutput<TRoute["response"]>>
  : (args: RouteArgs<TRoute>) => Promise<InferOutput<TRoute["response"]>>;

function interpolatePath(
  pathTemplate: string,
  params: Record<string, unknown> | undefined,
): string {
  if (!params) {
    return pathTemplate;
  }

  return pathTemplate.replace(/:([A-Za-z0-9_]+)/g, (_match, paramName: string) => {
    const value = params[paramName];

    if (value === undefined || value === null) {
      throw new Error(`Missing path parameter '${paramName}'.`);
    }

    return encodeURIComponent(String(value));
  });
}

function buildRouteArgs(route: Route, params: unknown, query: unknown, body: unknown): unknown {
  const args: ClientRequestArgs = {};

  if (route.params) {
    args.params = params;
  }
  if (route.query) {
    args.query = query;
  }
  if (route.request) {
    args.body = body;
  }

  return Object.keys(args).length === 0 ? undefined : args;
}

function invokeOverride<TRoute extends Route>(
  override: RouteMethod<TRoute>,
  args: unknown,
): Promise<InferOutput<TRoute["response"]>> {
  return (override as (args?: RouteArgs<TRoute>) => Promise<InferOutput<TRoute["response"]>>)(
    args as RouteArgs<TRoute> | undefined,
  );
}

async function ensureOk(response: Response): Promise<void> {
  if (response.ok) {
    return;
  }

  const errorBody = await response.text();
  throw new Error(`Request failed with ${response.status}: ${errorBody}`);
}