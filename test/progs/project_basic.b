rel point {
    x: real,
    y: real,
}

p: point;

rel point_res {
    x: real,
}

fn s_project(): point_res
{
    return p project(x);
}
