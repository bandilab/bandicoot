rel point {
    x: real,
    y: real,
}

a: point;
b: point;

rel point_res {
    r: real,
}

fn test(): point_res
{
    return a - b;
}
