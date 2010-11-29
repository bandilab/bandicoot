rel point {
    x: real,
    y: real,
}

rel user {
    id: int,
    name: string,
}

p: point;
u: user;

fn cannot_assign()
{
    p = u;
}
