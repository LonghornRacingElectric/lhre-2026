import pandas as pd
import numpy as np


class QueryBuilder:
    '''
        This class is designed to make project-specific SQL queries easier to construct and manipulate.

        Sample query build:
        query = (QueryBuilder()
                    .select('te.id as te_id', 'te.individualplayerid as user_id')
                    .from_table('team_event', 'te')
                    .join({'type': 'LEFT', 'table': 'training_player_analyze', 'abbreviation': 'tpa',
                           'on': 'tpa.trainingid == te.id'})
                    .join({'table': 'ddd', 'on': 'ddd.teameventid = te.id AND ddd.userid = tpa.userid'})
                    .where(['te.source = "squad"', 'te.type = 1'])
                    .limit(100)
                ).get_query()
        or for query + data retrieval:
                ).send_query(pd.Series)
    '''
    def __init__(self):
        ''' QueryBuilder constructor creates empty strings for methods to populate and can hold a DBHandler object '''
        self.__select = 'SELECT '
        self.__from = 'FROM '
        self.__join = ''
        self.__where = ''
        self.__groupby = ''
        self.__order = ''
        self.__limit = ''

    def __repr__(self):
        return self.get_query('coerce')

    def select(self, *args):
        '''
            This method creates the SELECT section of the query
            Params:
                *args:  str         -->     String literal--either full select string or single columns
                        array-like  -->     Array of string literals which are re-passed as separate args
        '''
        for arg in args:
            if isinstance(arg, str):
                if ',' not in arg:
                    arg += ','
                if arg[:-1] != ' ':
                    arg += ' '
                self.__select += arg
            elif isinstance(arg, (tuple, list, np.ndarray)):
                self.select(*arg)
            else:
                raise NotImplementedError(f'Type {type(arg)} input not implemented yet. Use string or array-like.')
        if self.__select[-2:] == ', ':
            self.__select = self.__select[:-2]
        if self.__select[-1:] != '\n':
            self.__select += '\n'

        return self

    def from_table(self, table: str, abbreviation: str = ''):
        '''
            This method creates the FROM section of the query
            Params:
                table:          str     -->     String literal representing the table name
                abbreviation:   str     -->     String literal representing the abbreviation of the table name
        '''
        self.__from += f'{table} {abbreviation}\n'

        return self

    def join(self, *args):
        '''
            This method creates the JOIN section of the query
            Params:
                *args:      str         -->     Full string literal (little to no preprocessing) used to make join
                            dict        -->     Dict used to create join.
                                                Keys:
                                                    type | join:          str          -->    Type of join to use (LEFT, INNER, etc.)
                                                    table | name:         str          -->    Name of table to join
                                                    abbreviation | abb:   str          -->    Abbreviation for join table
                                                    on:                   list | str   -->    Condition(s) to join table on
                            array-like  -->     Array of string literals which are re-passed as separate args
        '''
        for arg in args:
            if isinstance(arg, str):
                if 'JOIN' not in arg.upper():
                    arg = 'JOIN ' + arg
                self.__join += f'{arg}\n'
            elif isinstance(arg, dict):
                table_name = arg.get('table', arg.get('name'))
                if not table_name:
                    raise KeyError('Neither table nor name were in the join table dictionary.')

                join_type = arg.get('type', arg.get('join', ''))
                if join_type and join_type[:-1] != ' ':
                    join_type += ' '

                abbreviation = arg.get('abbreviation', arg.get('abb', ''))
                if abbreviation and abbreviation[:-1] != ' ':
                    abbreviation += ' '

                self.__join += f'{join_type}JOIN {table_name} {abbreviation}ON {arg.get("on") if isinstance(arg.get("on"), str) else " AND ".join(arg.get("on"))}\n'
            elif isinstance(arg, (tuple, list, np.ndarray)):
                self.join(*arg)
            else:
                raise NotImplementedError(f'Type {type(arg)} input not implemented yet. Use string or array-like.')
        if '"' in self.__join:
            self.__join = self.__join.replace('"', "'")
        if '==' in self.__join:
            self.__join = self.__join.replace('==', "=")

        return self

    def where(self, *args):
        '''
            This method creates the WHERE section of the query
            Params:
                *args       str         -->     String literal used to create where condition
                            array-like  -->     Array of string literals which are re-passed as separate args
        '''
        if not self.__where:
            self.__where = 'WHERE\n' + self.__where
        for arg in args:
            if isinstance(arg, str):
                self.__where += '\t' + ("AND " if self.__where != "WHERE\n" else "") + f'{arg}\n'
            elif isinstance(arg, (tuple, list, np.ndarray)):
                self.where(*arg)
            else:
                raise NotImplementedError(f'Type {type(arg)} input not implemented yet. Use string or array-like.')
        if '"' in self.__where:
            self.__where = self.__where.replace('"', "'")
        if '==' in self.__where:
            self.__where = self.__where.replace('==', "=")

        return self

    def groupby(self, *args):
        '''
            This method creates the GROUP BY section of the query
            Params:
                *args       str         -->     String literal used to create group by condition
                            array-like  -->     Array of string literals which are re-passed as separate args
        '''
        if not self.__groupby:
            self.__groupby += 'GROUP BY '
        for arg in args:
            if isinstance(arg, str):
                self.__groupby += ('' if self.__groupby == 'GROUP BY ' else ', ') + arg
            elif isinstance(arg, (tuple, list, np.ndarray)):
                self.groupby(*arg)
        if self.__groupby[-1:] != '\n':
            self.__groupby += '\n'

        return self

    def order(self, *args, desc: bool = True):
        '''
            This method creates the ORDER BY section of the query
            Params:
                *args       str         -->     String literal used to create order by condition
                            array-like  -->     Array of string literals which are re-passed as separate args
                desc        bool        -->     Determines whether order should be descending or ascending
        '''
        if not self.__order:
            self.__order += 'ORDER BY '
        for arg in args:
            if isinstance(arg, str):
                self.__order += ('' if self.__order == 'ORDER BY ' else ', ') + arg
            elif isinstance(arg, (tuple, list, np.ndarray)):
                self.order(*arg)

        if self.__order[-1:] != '\n':
            self.__order += (' DESC' if desc else ' ASC') + '\n'
        else:
            self.__order = self.__order[:-1] + (' DESC' if desc else ' ASC') + '\n'

        return self

    def limit(self, val: int = None):
        '''
            This method creates the LIMIT section of the query
            Params:
                val:        int         -->     Number of rows to limit the query to
        '''
        if self.__limit:
            raise ValueError('Limit has already been set. Please only use limit method once.')
        if val:
            self.__limit = f'LIMIT {val}'

        return self

    def get_query(self, limit: str = 'error'):
        '''
            This method constructs the query using the several partial query strings
            Params:
                limit:      ['error', 'coerce', 'ignore']      -->     Determines how to deal with a lack of limit
        '''
        query = ''.join(self.__dict__.values())
        if 'LIMIT' not in query.upper():
            if limit == 'error':
                raise ValueError('QUERY IS MISSING LIMIT. Either add limit or change limit arg to "coerce".')
            elif limit not in ['coerce', 'ignore']:
                raise NotImplementedError('Unknown limit handling input. Either use "error" or "coerce".')
        return query

    def send_query(self, return_type: type = pd.DataFrame, limit: str = 'error', handler=None, **kwargs):
        '''
            This method creates and sends the query and then puts it into a specific dtype
            Params:
                return_type:    class                           -->     Return type for data (pd.DataFrame, pd.Series,
                                                                                              dict, list, np.ndarray)
                limit:          ['error', 'coerce', 'ignore']   -->     Determines how to deal with a lack of limit
                handler:        DBHandler | AlgoDBHandler       -->     Handler to pass into sql_query if
                **kwargs:                                       -->     Input for pd.Series or pd.DataFrame constructor and sql_query
        '''
        num_cols = self.__select.count(',') + 1
        if return_type is pd.Series:
            if num_cols == 1:
                raise ValueError('Cannot form Series from single column.')
            elif num_cols > 2:
                raise ValueError('Cannot form Series from more than two columns.')
        elif return_type is dict:
            if num_cols == 1:
                raise ValueError('Cannot form dict from single column.')

        data = sql_query(self.get_query(limit), db_handler=kwargs.pop('db_handler', handler),
                         db_name=kwargs.pop('db_name', None))

        if return_type is pd.DataFrame:
            return pd.DataFrame(data, columns=kwargs.pop('columns', kwargs.pop('cols', None)),
                                index=kwargs.pop('index', None), dtype=kwargs.pop('dtype', None),
                                copy=kwargs.pop('copy', None))
        elif return_type is pd.Series:
            return pd.Series({k: v for k, v in [row.values() for row in data]},
                             dtype=kwargs.pop('dtype', None), name=kwargs.pop('name', None),
                             copy=kwargs.pop('copy', False))
        elif return_type is dict:
            if num_cols == 2:
                return {k: v for k, v in [row.values() for row in data]}
            return pd.DataFrame(data).to_dict(kwargs.pop('orient', 'dict'))
        elif return_type is np.ndarray:
            return np.array([list(row.values()) for row in data])
        elif return_type is list:
            return [list(row.values()) for row in data]
        elif return_type == 'raw':
            return data
        else:
            raise NotImplementedError(f'Send query has not been implemented for {return_type} yet.')

    @staticmethod
    def manual_query(query: str, return_type: type = pd.DataFrame, handler=None, **kwargs):
        '''
            This static method can be used to get the return type
            functionality of the instance method with a pre-built query
            Params:
                query:          str                             -->     Query to send to database
                return_type:    class                           -->     Return type for data (pd.DataFrame, pd.Series,
                                                                                              dict, list, np.ndarray)
                limit:          ['error', 'coerce', 'ignore']   -->     Determines how to deal with a lack of limit
                handler:        DBHandler | AlgoDBHandler       -->     Handler to pass into sql_query if
                **kwargs:                                       -->     Input for pd.Series or pd.DataFrame constructor and sql_query
        '''
        num_cols = query[:query.index('FROM')].count(',') + 1
        if return_type is pd.Series:
            if num_cols == 1:
                raise ValueError('Cannot form Series from single column.')
            elif num_cols > 2:
                raise ValueError('Cannot form Series from more than two columns.')
        elif return_type is dict:
            if num_cols == 1:
                raise ValueError('Cannot form dict from single column.')

        data = sql_query(query, db_handler=kwargs.pop('db_handler', handler),
                         db_name=kwargs.pop('db_name', None))

        if return_type is pd.DataFrame:
            return pd.DataFrame(data, columns=kwargs.pop('columns', kwargs.pop('cols', None)),
                                index=kwargs.pop('index', None), dtype=kwargs.pop('dtype', None),
                                copy=kwargs.pop('copy', None))
        elif return_type is pd.Series:
            return pd.Series({k: v for k, v in [row.values() for row in data]},
                             dtype=kwargs.pop('dtype', None), name=kwargs.pop('name', None),
                             copy=kwargs.pop('copy', False))
        elif return_type is dict:
            if num_cols == 2:
                return {k: v for k, v in [row.values() for row in data]}
            return pd.DataFrame(data).to_dict(kwargs.pop('orient', 'dict'))
        elif return_type is np.ndarray:
            return np.array([list(row.values()) for row in data])
        elif return_type is list:
            return [list(row.values()) for row in data]
        elif return_type == 'raw':
            return data
        else:
            raise NotImplementedError(f'Send query has not been implemented for {return_type} yet.')


if __name__ == '__main__':
    data = (QueryBuilder()
                .select('te.id as te_id', 'te.individualplayerid as user_id')
                .from_table('team_event', 'te')
                .join({'type': 'LEFT', 'table': 'training_player_analyze', 'abbreviation': 'tpa',
                       'on': 'tpa.trainingid == te.id'})
                .join('JOIN ddd ON ddd.teameventid = te.id AND ddd.userid = tpa.userid')
                .where('te.type = 1')
                # .where('te.source = "uno"')
                .limit(100)
            ).send_query(pd.Series)
    print(data)