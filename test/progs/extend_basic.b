rel point {
    x: real,
    y: real
}

p: point;

fn test(): rel {x: real, y: real, color: int}
{
    return p extend(color = 0);
}

fn test2(c: int): rel {x: real, y: real, color: int}
{
    return p extend(color = c);
}
