type point {x real, y real}

type user {x int}

var p point;

var u user;

fn cannot_assign() void
{
	p *= u;
}
