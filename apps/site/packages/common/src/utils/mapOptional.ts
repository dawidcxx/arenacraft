import { isNil } from "./isNil";

export function mapOptional<In, Out>(
  maybeNil: In | undefined | null,
  mappingOp: (item: In) => Out,
): Out | null {
  return isNil(maybeNil) ? null : mappingOp(maybeNil);
}