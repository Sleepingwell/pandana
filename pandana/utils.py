import pandas as pd


def reindex(series1, series2):
    """
    Reindex the first series by the second series.

    Parameters
    ----------
    series1 : pandas.Series
        Pandas series to reindex
    series2 : pandas.Series
        Pandas series to set the index of series1 by

    Returns
    -------
    df.right : pandas.DataFrame
    """

    df = pd.merge(pd.DataFrame({"left": series2}),
                  pd.DataFrame({"right": series1}),
                  left_on="left",
                  right_index=True,
                  how="left")
    return df.right
