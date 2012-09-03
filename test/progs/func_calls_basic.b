type X {i int}

var gv1 gv2 X;

fn myfunc0(x X) void
{
    gv2 = x;
}

fn myfunc1() X
{
    myfunc0 gv1;
    return gv1;
}

fn myfunc2(x X) void
{
    gv1 = x;
    gv2 = x;
}

fn Test() X
{
    myfunc2 gv1;
    return myfunc1;
}

fn Test2(x X) void
{
    var y = x;
}
