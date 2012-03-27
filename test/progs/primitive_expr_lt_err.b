type test {i int, r real}

var t test;

fn s_lt_err() test
{
	return (select r < i t);
}
