rel point {
    x: real,
    y: real,
}

fn svc_basic(p: point): point
{
    return p;
}

fn inline(p: rel {x: real, y: real}): point
{
    return p;
}
