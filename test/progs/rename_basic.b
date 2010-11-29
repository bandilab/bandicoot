rel point {
    x: real,
    y: real,
}

p: point;

rel point_res {
    x_coord: real,
    y_coord: real,
}

fn s_rename(): point_res
{
    return p rename(x_coord = x, y_coord = y);
}
