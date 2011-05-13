rel point {
    x: real,
    y: real,
}

fn test(p: point): point
{
    return p select(x < unknownFunc(y));
}
