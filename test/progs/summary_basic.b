type Pos {pos int}

type Time {when long}

var pos Pos;

var when Time;

type Dims {min_time long, max_time long, min_pos int, max_pos int, avg_pos real, add_pos int}

fn dim1() Dims
{
	var t = (extend id = 1 when);
	var p = (extend id = 1 pos);
	var st = (summary min_time = (min when 0L), max_time = (max when -3L) t (project id t));
	var sp = (summary min_pos = (min pos 0), max_pos = (max pos 0), avg_pos = (avg pos 0.0), add_pos = (add pos 0) p (project id p));

	return (join st sp);
}

fn dim2() Dims
{
	return (join (summary min_time = (min when 0L), max_time = (max when 0L) when) (summary min_pos = (min pos 0), max_pos = (max pos 0), avg_pos = (avg pos 0.0), add_pos = (add pos 0) pos));
}

type Queue {order int}

var queue Queue;

fn next() {order int}
{
	return (rename order = norder (project norder (extend norder = order + 1 (summary order = (max order 0) queue))));
}

fn count() {count int}
{
	return (summary count = cnt queue);
}

fn SumPrecedenceCheck() void
{
	var t = (extend id = 1 when);
	var st = (select min_time > 0L (summary min_time = (min when 0L), max_time = (max when -3L) t (project id t)));
}
