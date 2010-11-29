rel point { x: real, y: real }

p1: point;
p2: point;

store: point;

fn hello(): point
{
    tmp := p1 - p2;
    store = tmp;

    tmp := store + p1 + p2;
    return tmp;
}
