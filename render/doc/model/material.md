# Material System (Descriptor Set)

## Layout

| Binding |     Type      |             Description             |
| :-----: | :-----------: | :---------------------------------: |
|    0    |   `Buffer`    | [Parameter Array](#parameter-array) |
|    1    | `Sampler2D[]` |   [Texture Array](#texture-array)   |

## Parameter Array

```
+----------+--------------+
|          | Albedo Index +
| Material +--------------+
|          | Normal Index +
|  Params  +--------------+
|          | ...          |
+----------+--------------+
```

An array of texture parameters.

- Texture Index: provide indices into the [texture array](#texture-array)
- Material Parameter: Material rendering configs, e.g. albedo-multiplier

## Texture Array

```
+---------+
| Texture |
+---------+
| Texture |
+---------+
| Texture |
+---------+
| ...     |
|         |
```

An array of textures. See [Indexing](#indexing) for indexing

## Indexing

1. Material Info: the first element stores the default material, the following ones stores Material 0, ,Material 1 etc.
2. Texture: use corresponding indices in `texture_index` struct to find in the texture array

```
                            [Binding 0]                        [Binding 1]
                    +----------+--------------+                +---------+
                    |          | Albedo Index |>--+----------->| Texture |
                    | Material +--------------+   |            +---------+
  [ID = <none>] --->|          | Normal Index |>--)----------->| Texture |
                    |  Params  +--------------+   |            +---------+
                    |          | ...          |   |     +----->| Texture |
                    +----------+--------------+   |     |      +---------+
                    |          | Albedo Index |>--+     |      | Texture |
                    | Material +--------------+         |      +---------+
  [ID = 0] -------->|          | Normal Index |>--------+      | Texture |
                    |  Params  +--------------+         |      +---------+
                    |          | ...          |         |      | Texture |
                    +----------+--------------+         |      +---------+
                    |          | Albedo Index |>--------)----->| Texture |
                    | Material +--------------+         |      +---------+
  [ID = 1] -------->|          | Normal Index |>--------+      | Texture |
                    |  Params  +--------------+                +---------+
                    |          | ...          |                | Texture |
                    +----------+--------------+                +---------+
                    |        ...              |                | ...     |
                    |                         |                |         |
```
