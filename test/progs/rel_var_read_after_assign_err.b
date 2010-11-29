rel point { x: real, y: real }

gp: point;

fn test(p: point): point
{
    gp = p;
    return gp;
}
