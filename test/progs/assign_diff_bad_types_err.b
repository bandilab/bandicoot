rel point {
    x: real,
    y: real,
}

rel user {
    x: int,
    y: string,
}

p: point;
u: user;

fn cannot_assign()
{
    p -= u;
}
