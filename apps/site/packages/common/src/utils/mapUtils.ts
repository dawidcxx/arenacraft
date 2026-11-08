export function getOrPut<K, V>(map: Map<K, V>, key: K, defaultValue: V): V {
  if (!map.has(key)) {
    map.set(key, defaultValue);
  }
  return map.get(key)!;
}

export function getOrPutLazy<K, V>(map: Map<K, V>, key: K, defaultValueFactory: () => V): V {
  if (!map.has(key)) {
    map.set(key, defaultValueFactory());
  }
  return map.get(key)!;
}