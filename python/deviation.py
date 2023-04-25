import tushare as ts
import pandas as pd
from datetime import datetime, timedelta

def get_index_data(exchange_code, index_code):
    """
    获取指数数据，返回当日收盘价
    """
    index_data = ts.get_hist_data(index_code, index=True, start=datetime.now().strftime('%Y-%m-%d'), end=datetime.now().strftime('%Y-%m-%d'))
    return index_data['close'][0]

def get_deviation(stock_code, days, percent):
    """
    获取股票与所属指数偏离值，返回偏离百分比
    """
    if stock_code.startswith('6'):  # 上证指数
        exchange_code = 'sh'
        index_code = '000001'
    elif stock_code.startswith('00'):  # 深证成指
        exchange_code = 'sz'
        index_code = '399001'
    elif stock_code.startswith('30'):  # 创业板指
        exchange_code = 'sz'
        index_code = '399006'
    else:
        raise ValueError("无法判断股票所属交易所！")
        
    index_close = get_index_data(exchange_code, index_code)
    stock_data = ts.get_hist_data(stock_code, start=(datetime.now() - timedelta(days=days)).strftime('%Y-%m-%d'), end=datetime.now().strftime('%Y-%m-%d'))
    if stock_data is None:
        return None
    stock_close = stock_data['close'].mean()
    deviation = (stock_close - index_close) / index_close
    deviation_percent = deviation * 100
    return deviation_percent

def get_deviation_top(percent, days):
    """
    获取最近N个交易日偏离值达到指定百分比的股票列表
    """
    stock_list = ts.get_stock_basics().index.tolist()
    deviation_list = []
    for stock_code in stock_list:
        deviation = get_deviation(stock_code, days, percent)
        if deviation is not None and abs(deviation) >= percent:
            deviation_list.append({'code': stock_code, 'deviation': deviation})
    deviation_df = pd.DataFrame(deviation_list)
    deviation_df = deviation_df.sort_values(by='deviation', ascending=False)
    deviation_df.index = range(1, len(deviation_df)+1)
    return deviation_df

# 输出最近10个交易日偏离值达到100%的股票列表
deviation_df = get_deviation_top(100, 10)
print(deviation_df)

# 输出最近30个交易日偏离值达到200%的股票列表
deviation_df = get_deviation_top(200, 30)
print(deviation_df)
