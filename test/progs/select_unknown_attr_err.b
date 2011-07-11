rel point {
    x: int,
    y: int,
}

fn test(p: point): point
{
    return p select(z > 10);
}
