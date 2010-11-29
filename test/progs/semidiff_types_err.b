rel point {
    x: real,
    y: real,
}

rel address {
    street: string,
    building: int,
}

p: point;
a: address;

fn cannot_semidiff(): point
{
    return p - a;
}
