type test {r real, s string}

var t test;

fn s_eq_err() test
{
	return (select r == s t);
}
