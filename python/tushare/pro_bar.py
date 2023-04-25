import tushare as ts
import pandas as pd
from datetime import datetime, timedelta

ts.set_token('ded7e7ca622a0dea6b6fc3adf45580ec8423eec38eaa5ae6f0c5b860')

pro = ts.pro_api()

# 判断股票所属交易所
def get_deviation(stock_code):
    if stock_code.startswith('6'):  # 上证指数
        exchange_code = 'sh'
        index_code = '000001'
        return index_code
    elif stock_code.startswith('00'):  # 深证成指
        exchange_code = 'sz'
        index_code = '399001'
        return index_code
    elif stock_code.startswith('30'):  # 创业板指
        exchange_code = 'sz'
        index_code = '399006'
        return index_code
    else:
        return None


# 交易日历
def get_trade_cal(days):
    trade_cal_df = pro.trade_cal(exchange='SSE', start_date=(datetime.now() - timedelta(days)).strftime('%Y%m%d'), end_date=datetime.now().strftime('%Y%m%d'))
    trade_cal_df = trade_cal_df[trade_cal_df['is_open'] != 0]
    print(trade_cal_df)

# get_trade_cal(90)

stock_df = pro.stock_basic(exchange='', list_status='L', fields='ts_code,symbol,name,area,industry,list_date').loc[:, "ts_code"]




# print(stock_df)

df = ts.pro_bar(ts_code='000001.SH', asset='I', start_date=(datetime.now() - timedelta(30)).strftime('%Y%m%d'), end_date=datetime.now().strftime('%Y%m%d'))

# print(df)