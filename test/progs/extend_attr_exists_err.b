rel point {
    x: real,
    y: real,
}

p: point;

fn test(): point
{
    return p extend(x = "replacing an attribute");
}
