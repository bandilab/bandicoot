rel point {
    x: real,
    y: real,
}

p: point;

fn cannot_rename(): point
{
    return p rename(x_coord = z, y_coord = y);
}
