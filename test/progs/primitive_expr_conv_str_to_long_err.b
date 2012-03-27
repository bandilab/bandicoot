type T {s string}

fn test(t T) T
{
	return (select 5L > (long s) t);
}
