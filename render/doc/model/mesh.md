# Mesh Data Layout

## Vertex Array

```
+--------------------+--------------------+--------------------+-----+
| Primitive 0        | Primitive 1        | Primitive 2        | ... |
+----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
| v0 | v1 | v2 | ... | v0 | v1 | v2 | ... | v0 | v1 | v2 | ... | ... |
+----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
```

- Stores vertices of all primitives contiguously

## Index Array

```
+--------------------+--------------------+--------------------+-----+
| Primitive 0        | Primitive 1        | Primitive 2        | ... |
+----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
| i0 | i1 | i2 | ... | i0 | i1 | i2 | ... | i0 | i1 | i2 | ... | ... |
+----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
```

- Stores indices of all primitives contiguously
- All indices are local/relative, which means they are offsets relative to the first vertex of their primitive

## Primitive Attribute Array

```
        (Vertex Array)
        +--------------------+--------------------+--------------------+-----+
        | Primitive 0        | Primitive 1        | Primitive 2        | ... |
        +----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
        | v0 | v1 | v2 | ... | v0 | v1 | v2 | ... | v0 | v1 | v2 | ... | ... |
        +----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
                               ^
                               |
    +--------------------------+
    |
    |   (Index Array)
    |   +--------------------+--------------------+--------------------+-----+
    |   | Primitive 0        | Primitive 1        | Primitive 2        | ... |
    |   +----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
    |   | i0 | i1 | i2 | ... | i0 | i1 | i2 | ... | i0 | i1 | i2 | ... | ... |
    |   +----+----+----+-----+----+----+----+-----+----+----+----+-----+-----+
    |                          ^ |_______________|
    |       +---------------+  |         |
    |       | Index offset  |>-+         |
    +------<| Vertex offset |            |
            | Vertex count  |>-----------+       +---------------+
            | Material ID   |>-----------------> | Material Info |
            | AABB min      |                    +---------------+
            | AABB max      |
            +---------------+
                       ^
                       |
                       |
        +---------+---------+---------+-----+
        | Attr[0] | Attr[1] | Attr[2] | ... | (Primitive Attribute Array)
        +---------+---------+---------+-----+
```

- Stores static data for a specific primitive contiguously
- Each entry includes:
    - **Index Offset**: Global offset of the first index into the index array
    - **Vertex Offset**: Global offset of the first vertex into the vertex array
    - **Vertex Count**: Total vertex count of the primitive
    - **Material ID**: Material ID of the primitive, used to find the correct `MaterialInfo` in [Material Array](material.md). The ID is represented using a 32-bit unsigned integer, and the special value of `0xFFFFFFFF` is used to represent fallback material.
    - **AABB min/max**: Min/max positions of **local** AABB bounding box

> Future work:
>
> - Store index offsets and vertex counts for different LOD levels
