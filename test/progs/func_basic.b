rel point {
    x: real,
    y: real,
}

temperature: point;
unemployment: point;
currency: point;

fn get_pointless_results(p: point): point
{
    return (temperature + currency * unemployment) - currency * temperature + p;
}

fn empty()
{
}
