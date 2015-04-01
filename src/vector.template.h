#pragma once

typedef struct $<name_2>_s
{
    union
    {
        struct
        {
            $<type> x;
            $<type> y;
        };
        struct
        {
            $<type> w;
            $<type> h;
        };
        $<type> d[2];
    };
} $<name_2>;

static $<name_2> make_$<name_2>($<type> x, $<type> y)
{
    $<name_2> result;
    result.x = x;
    result.y = y;
    return result;
}

typedef struct $<name_3>_s
{
    union
    {
        struct
        {
            $<type> x;
            $<type> y;
            $<type> z;
        };
        $<type> d[3];
    };
} $<name_3>;

static $<name_3> make_$<name_3>($<type> x, $<type> y, $<type> z)
{
    $<name_3> result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}
