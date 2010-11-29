rel point {
    x: real,
    y: real,
}

p: point;

fn cannot_rename(): point
{
    return p rename(x_coord = x, x_coord = y);
}
