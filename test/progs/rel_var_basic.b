type point {x real, y real}

var g1 point;

var g2 point;

fn test(p point) void
{
	g1 = (union g1 (join g2 p));
	g2 = (union g2 p);
}
