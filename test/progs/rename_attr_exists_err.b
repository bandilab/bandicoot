type point {x real, y real, when int}

var p point;

fn cannot_rename() point
{
	return (rename x_coord = x, when = y p);
}
