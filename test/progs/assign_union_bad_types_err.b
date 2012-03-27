type point {x real, y real}

type user {id int, name string}

var p point;

var u user;

fn cannot_assign() void
{
	p += u;
}
