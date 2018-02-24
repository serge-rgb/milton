// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

typedef DArray<int> arri;

int
milton_main()
{
    arri x = {};

    for ( u64 i = 0; i < 10; ++i ) {
        push(&x, (int)i);
    }
    for ( u64 i = 0; i < 10; ++i ) {
        int d = x.data[i];
        mlt_assert (d == (int)i);
    }

    auto y = dynamic_array<int>(2);

    for ( int i=0; i < 10; ++i ) {
        push(&y, i);
    }

    release(&x);
    release(&y);

    return 0;
}
