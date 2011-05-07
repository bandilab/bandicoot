rel point {
    x: real,
    y: real,
}

p1: point;

rel key {
    x: real,
}

p2: key;

fn s_assign()
{
    p1 -= p2;
}
