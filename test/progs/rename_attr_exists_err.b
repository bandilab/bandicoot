rel point {
    x: real,
    y: real,
    time: int,
}

p: point;

fn cannot_rename(): point
{
    return p rename(x_coord = x, time = y);
}
