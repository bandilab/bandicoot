rel point { x: real, y: real }

g1: point;
g2: point;

fn test(p: point)
{
    g1 = g1 + g2 * p;
    g2 = g2 + p;
}
