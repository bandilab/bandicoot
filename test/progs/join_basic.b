type 2Dpoint {x real, y real}

type 3Dpoint {x real, y real, z real}

var p1 2Dpoint;

var p2 3Dpoint;

fn s_join() 3Dpoint
{
	return (join p1 p2);
}
