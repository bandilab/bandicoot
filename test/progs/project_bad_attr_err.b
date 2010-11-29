rel point {
    x: real,
    y: real,
}

p: point;

rel point_res {
    x: real,
    z: real,
}

fn cannot_project(): point_res
{
    return p project(x, z);
}
