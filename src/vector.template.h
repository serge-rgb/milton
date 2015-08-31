typedef struct $<Name2>_s
{
    union
    {
        struct
        {
            $<Type> x;
            $<Type> y;
        };
        struct
        {
            $<Type> w;
            $<Type> h;
        };
        $<Type> d[2];
    };
} $<Name2>;

func $<Name2> sub_$<Name2> ($<Name2> a, $<Name2> b)
{
    $<Name2> result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

func $<Name2> add_$<Name2> ($<Name2> a, $<Name2> b)
{
    $<Name2> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

func $<Name2> mul_$<Name2> ($<Name2> a, $<Name2> b)
{
    $<Name2> result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

func $<Name2> scale_$<Name2> ($<Name2> v, $<Type> factor)
{
    $<Name2> result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

func $<Name2> invscale_$<Name2> ($<Name2> v, $<Type> factor)
{
    $<Name2> result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

func $<Name2> perpendicular_$<Name2> ($<Name2> a)
{
    $<Name2> result =
    {
        -a.y,
        a.x
    };
    return result;
}

typedef struct $<Name3>_s
{
    union
    {
        struct
        {
            $<Type> x;
            $<Type> y;
            $<Type> z;
        };
        struct
        {
            $<Type> r;
            $<Type> g;
            $<Type> b;
        };
        struct
        {
            $<Type> h;
            $<Type> s;
            $<Type> v;
        };
        $<Type> d[3];
    };
} $<Name3>;

func $<Name3> sub_$<Name3> ($<Name3> a, $<Name3> b)
{
    $<Name3> result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;
    return result;
}

func $<Name3> add_$<Name3> ($<Name3> a, $<Name3> b)
{
    $<Name3> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;
    return result;
}

func $<Name3> mul_$<Name3> ($<Name3> a, $<Name3> b)
{
    $<Name3> result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    return result;
}

func $<Name3> scale_$<Name3> ($<Name3> v, $<Type> factor)
{
    $<Name3> result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    result.z = factor * v.z;
    return result;
}

func $<Name3> invscale_$<Name3> ($<Name3> v, $<Type> factor)
{
    $<Name3> result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    result.z = v.z / factor;
    return result;
}

typedef struct $<Name4>_s
{
    union
    {
        struct
        {
            $<Type> x;
            $<Type> y;
            $<Type> z;
            $<Type> w;
        };
        struct
        {
            $<Type> r;
            $<Type> g;
            $<Type> b;
            $<Type> a;
        };
        union
        {
            $<Name3> rgb;
            $<Type>  _pad__a;
        };
        $<Type> d[4];
    };
} $<Name4>;
