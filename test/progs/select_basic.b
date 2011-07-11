rel point {
    x: int,
    y: int,
}

fn test(p: point): point
{
    return p select(x > 10);
}

p: point;

fn test2(i: int): point
{
    return p select(x > i);
}
