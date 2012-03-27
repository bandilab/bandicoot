type test {i int, s string}

var t test;

fn s_gt_err() test
{
	return (select s > i t);
}
