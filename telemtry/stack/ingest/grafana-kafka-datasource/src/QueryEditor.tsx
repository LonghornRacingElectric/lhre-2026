import { debounce, type DebouncedFunc } from 'lodash';
import React, { ChangeEvent, PureComponent } from 'react';
import {
  InlineField,
  InlineFieldRow,
  Input,
  Select,
  MultiSelect,
  Button,
  Spinner,
  Alert,
  InlineLabel,
  useStyles2,
} from '@grafana/ui';
import { QueryEditorProps, GrafanaTheme2 } from '@grafana/data';
import { css } from '@emotion/css';
import { DataSource } from './datasource';
import {
  defaultQuery,
  KafkaDataSourceOptions,
  KafkaQuery,
  AutoOffsetReset,
  TimestampMode,
  AvroSchemaSource,
  ProtobufSchemaSource,
  MessageFormat,
  KeyFormat,
  FilterCondition,
  FilterOperator,
} from './types';

// Constants for Last N messages input
const LAST_N_MIN = 1;
const LAST_N_MAX = 1000000;
const LAST_N_DEFAULT = 100;

const autoResetOffsets: Array<{ label: string; value: AutoOffsetReset }> = [
  { label: 'Latest', value: AutoOffsetReset.LATEST },
  { label: 'Last N messages', value: AutoOffsetReset.LAST_N },
  { label: 'Earliest', value: AutoOffsetReset.EARLIEST },
];

const timestampModes: Array<{ label: string; value: TimestampMode }> = [
  { label: 'Kafka Event Time', value: TimestampMode.Message },
  { label: 'Dashboard received time', value: TimestampMode.Now },
];

const avroSchemaSources: Array<{ label: string; value: AvroSchemaSource }> = [
  { label: 'Schema Registry', value: AvroSchemaSource.SCHEMA_REGISTRY },
  { label: 'Inline Schema', value: AvroSchemaSource.INLINE_SCHEMA },
];

const protobufSchemaSources: Array<{ label: string; value: ProtobufSchemaSource }> = [
  { label: 'Schema Registry', value: ProtobufSchemaSource.SCHEMA_REGISTRY },
  { label: 'Inline Schema', value: ProtobufSchemaSource.INLINE_SCHEMA },
];

const messageFormats: Array<{ label: string; value: MessageFormat }> = [
  { label: 'JSON', value: MessageFormat.JSON },
  { label: 'Avro', value: MessageFormat.AVRO },
  { label: 'Protobuf', value: MessageFormat.PROTOBUF },
];

const keyFormats: Array<{ label: string; value: KeyFormat }> = [
  { label: 'None', value: KeyFormat.NONE },
  { label: 'String', value: KeyFormat.STRING },
  { label: 'JSON', value: KeyFormat.JSON },
  { label: 'Base64', value: KeyFormat.BASE64 },
];

const filterOperators: Array<{ label: string; value: FilterOperator }> = [
  { label: '=', value: FilterOperator.EQUALS },
  { label: '!=', value: FilterOperator.NOT_EQUALS },
  { label: '>', value: FilterOperator.GREATER_THAN },
  { label: '<', value: FilterOperator.LESS_THAN },
  { label: '>=', value: FilterOperator.GREATER_THAN_OR_EQUAL },
  { label: '<=', value: FilterOperator.LESS_THAN_OR_EQUAL },
  { label: 'contains', value: FilterOperator.CONTAINS },
  { label: 'not contains', value: FilterOperator.NOT_CONTAINS },
  { label: 'regex', value: FilterOperator.REGEX },
];

const partitionOptions: Array<{ label: string; value: number | 'all' }> = [{ label: 'All partitions', value: 'all' }];

const getStyles = (theme: GrafanaTheme2) => {
  return {
    topicField: css({
      minWidth: theme.spacing(32.5),
    }),
    partitionField: css({
      minWidth: theme.spacing(25),
    }),
    offsetField: css({
      minWidth: theme.spacing(32.5),
    }),
    schemaField: css({
      minWidth: theme.spacing(50),
    }),
    topicContainer: css({
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(1.25),
      position: 'relative',
      minWidth: theme.spacing(32.5),
    }),
    topicInputWrapper: css({
      position: 'relative',
      minWidth: theme.spacing(22.5),
    }),
    suggestionDropdown: css({
      position: 'absolute',
      top: '100%',
      left: 0,
      right: 0,
      backgroundColor: theme.colors.background.primary,
      border: `1px solid ${theme.colors.border.medium}`,
      borderRadius: theme.shape.radius.default,
      boxShadow: theme.shadows.z2,
      zIndex: theme.zIndex.dropdown,
      maxHeight: theme.spacing(25),
      overflowY: 'auto',
    }),
    suggestionItem: css({
      padding: theme.spacing(1, 1.5),
      cursor: 'pointer',
      fontSize: theme.typography.bodySmall.fontSize,
      color: theme.colors.text.primary,
      backgroundColor: 'transparent',
      '&:not(:last-child)': {
        borderBottom: `1px solid ${theme.colors.border.weak}`,
      },
      '&:hover': {
        backgroundColor: theme.colors.background.secondary,
      },
    }),
    fetchButton: css({
      minWidth: theme.spacing(7.5),
    }),
    successMessage: css({
      color: theme.colors.success.text,
      fontSize: theme.typography.bodySmall.fontSize,
    }),
    offsetContainer: css({
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(1),
    }),
    schemaValidationWrapper: css({
      display: 'flex',
      flexDirection: 'column',
      gap: theme.spacing(1),
      minWidth: theme.spacing(50),
    }),
    fileRow: css({
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(1),
      marginBottom: theme.spacing(1),
    }),
    hiddenFileInput: css({
      display: 'none',
    }),
    filenameText: css({
      fontSize: theme.typography.bodySmall.fontSize,
      color: theme.colors.text.secondary,
    }),
    textArea: css({
      width: '100%',
      fontFamily: theme.typography.fontFamilyMonospace,
      fontSize: theme.typography.bodySmall.fontSize,
      resize: 'vertical',
    }),
    validationMessage: css({
      marginTop: theme.spacing(0.5),
    }),
    validationLoading: css({
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(0.5),
    }),
    validationText: css({
      fontSize: theme.typography.bodySmall.fontSize,
      color: theme.colors.text.secondary,
    }),
    validationLabel: css({
      fontSize: theme.typography.bodySmall.fontSize,
    }),
    warningAlert: css({
      marginTop: theme.spacing(1),
    }),
    testConnectionContainer: css({
      display: 'flex',
      alignItems: 'center',
      gap: theme.spacing(1),
    }),
  };
};

type Props = QueryEditorProps<DataSource, KafkaQuery, KafkaDataSourceOptions>;

interface State {
  availablePartitions: number[];
  topicSuggestions: string[];
  showingSuggestions: boolean;
  loadingPartitions: boolean;
  partitionSuccess?: string;
  schemaValidation?: {
    status: 'valid' | 'invalid' | 'loading';
    message?: string;
  };
  protobufSchemaValidation?: {
    status: 'valid' | 'invalid' | 'loading';
    message?: string;
  };
  schemaRegistryValidation?: {
    status: 'valid' | 'invalid' | 'loading';
    message?: string;
  };
  selectedAvroFileName?: string;
  selectedProtobufFileName?: string;
  // Fields discovered by sampling recent messages from the topic
  availableFields: string[];
  discoveringFields: boolean;
  fieldDiscoveryError?: string;
}

// Wrapper component to use hooks
const QueryEditorWithStyles = (props: Props) => {
  const styles = useStyles2(getStyles);
  return <QueryEditorInner {...props} styles={styles} />;
};

interface QueryEditorInnerProps extends Props {
  styles: ReturnType<typeof getStyles>;
}

class QueryEditorInner extends PureComponent<QueryEditorInnerProps, State> {
  private debouncedSearchTopics: DebouncedFunc<(input: string) => void>;
  private debouncedRunQuery: DebouncedFunc<() => void>;
  private lastCommittedTopic = '';
  private fetchPartitionsTimeoutId?: ReturnType<typeof setTimeout>;
  private topicBlurTimeout?: ReturnType<typeof setTimeout>;
  private avroSchemaValidationTimeout?: ReturnType<typeof setTimeout>;
  private protobufSchemaValidationTimeout?: ReturnType<typeof setTimeout>;
  private avroFileInputRef?: React.RefObject<HTMLInputElement>;
  private protobufFileInputRef?: React.RefObject<HTMLInputElement>;

  constructor(props: QueryEditorInnerProps) {
    super(props);
    this.state = {
      availablePartitions: [],
      topicSuggestions: [],
      showingSuggestions: false,
      loadingPartitions: false,
      partitionSuccess: undefined,
      availableFields: [],
      discoveringFields: false,
    };

    this.debouncedSearchTopics = debounce(this.searchTopics, 300);
    // Add debounced version of query execution to handle rapid changes better
    this.debouncedRunQuery = debounce(() => {
      this.props.onRunQuery();
    }, 500);
    // Ref for hidden file input
    this.avroFileInputRef = React.createRef<HTMLInputElement>();
    this.protobufFileInputRef = React.createRef<HTMLInputElement>();
  }

  componentDidMount() {
    // If a saved panel already has a topic, allow manual fetch via button; do not auto fetch.
  }

  componentDidUpdate() {
    // No automatic partition fetching to avoid noisy errors while typing.
  }

  componentWillUnmount() {
    // Cancel debounced topic search to avoid setState after unmount
    this.debouncedSearchTopics.cancel();
    // Cancel debounced query execution to avoid issues after unmount
    this.debouncedRunQuery.cancel();
    if (this.fetchPartitionsTimeoutId) {
      clearTimeout(this.fetchPartitionsTimeoutId);
      this.fetchPartitionsTimeoutId = undefined;
    }
    if (this.topicBlurTimeout) {
      clearTimeout(this.topicBlurTimeout);
      this.topicBlurTimeout = undefined;
    }
    if (this.avroSchemaValidationTimeout) {
      clearTimeout(this.avroSchemaValidationTimeout);
      this.avroSchemaValidationTimeout = undefined;
    }
    if (this.protobufSchemaValidationTimeout) {
      clearTimeout(this.protobufSchemaValidationTimeout);
      this.protobufSchemaValidationTimeout = undefined;
    }
  }

  fetchPartitions = async () => {
    const { datasource, query, onChange, onRunQuery } = this.props;
    if (!query.topicName) {
      this.setState({ availablePartitions: [] });
      return;
    }
    this.setState({ loadingPartitions: true, partitionSuccess: undefined });
    try {
      const partitions = await datasource.getTopicPartitions(query.topicName);
      const partCount = (partitions || []).length;
      this.setState({
        availablePartitions: partitions || [],
        loadingPartitions: false,
        partitionSuccess: `Fetched ${partCount} partition${partCount === 1 ? '' : 's'}`,
      });

      // Auto-apply "all partitions" if not already set and trigger query
      if (query.partition === undefined || query.partition === null) {
        onChange({ ...query, partition: 'all' });
        onRunQuery();
      }

      // Clear success after 5s
      if (this.fetchPartitionsTimeoutId) {
        clearTimeout(this.fetchPartitionsTimeoutId);
        this.fetchPartitionsTimeoutId = undefined;
      }
      this.fetchPartitionsTimeoutId = setTimeout(() => {
        // Only clear if still mounted and timeout not replaced
        if (this.fetchPartitionsTimeoutId) {
          this.setState({ partitionSuccess: undefined });
          this.fetchPartitionsTimeoutId = undefined;
        }
      }, 5000);
    } catch (error) {
      console.error('Failed to fetch partitions:', error);
      // Rely on Grafana global error; do not show a duplicate inline error.
      this.setState({ availablePartitions: [], loadingPartitions: false });
    }
  };

  getPartitionOptions = (): Array<{ label: string; value: number | 'all' }> => {
    const options = [...partitionOptions];
    const query = { ...defaultQuery, ...this.props.query };
    const { partition } = query;
    const present = new Set<number>();
    (this.state.availablePartitions || []).forEach((p) => {
      options.push({ label: `Partition ${p}`, value: p });
      present.add(p);
    });
    // Ensure previously selected numeric partition is visible even before fetching
    if (typeof partition === 'number' && !present.has(partition)) {
      options.push({ label: `Partition ${partition}`, value: partition });
    }
    return options;
  };

  searchTopics = async (input: string) => {
    const { datasource } = this.props;
    const trimmed = input.trim();
    if (trimmed.length < 2) {
      this.setState({ topicSuggestions: [], showingSuggestions: false });
      return;
    }

    try {
      const topics = await datasource.searchTopics(trimmed, 10);
      const filteredTopics = (topics || [])
        .filter((topic) => topic.toLowerCase().startsWith(trimmed.toLowerCase()))
        .filter((topic) => {
          const hasStructure = /[-_.]/.test(topic);
          const isSignificantlyLonger = topic.length >= trimmed.length + 3;
          const isExactMatch = topic.toLowerCase() === trimmed.toLowerCase();
          return isExactMatch || isSignificantlyLonger || hasStructure;
        })
        .slice(0, 5);

      this.setState({ topicSuggestions: filteredTopics, showingSuggestions: filteredTopics.length > 0 });
    } catch (error) {
      console.error('Failed to search topics:', error);
      this.setState({ topicSuggestions: [], showingSuggestions: false });
    }
  };

  onTopicNameChange = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query } = this.props;
    const value = event.target.value;
    onChange({ ...query, topicName: value });
    // Do NOT run query yet; wait for Enter or blur to reduce noisy errors
    this.debouncedSearchTopics(value);
  };

  commitTopicIfChanged = () => {
    const query = { ...defaultQuery, ...this.props.query };
    const { topicName } = query;
    if (topicName && topicName !== this.lastCommittedTopic) {
      this.lastCommittedTopic = topicName;
      this.props.onRunQuery();
    }
  };

  onPartitionChange = (value: number | 'all') => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, partition: value });
    // Cancel any pending debounced query to prevent race conditions
    this.debouncedRunQuery.cancel();
    // Run query immediately for partition changes
    onRunQuery();
  };

  onAutoResetOffsetChanged = (value: AutoOffsetReset) => {
    const { onChange, query, onRunQuery } = this.props;
    // If switching to Last N and no lastN set, default to 100
    const next: KafkaQuery = { ...query, autoOffsetReset: value } as KafkaQuery;
    if (value === AutoOffsetReset.LAST_N && (!next.lastN || next.lastN <= 0)) {
      next.lastN = 100;
    }
    onChange(next);
    onRunQuery();
  };

  onLastNChanged = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query, onRunQuery } = this.props;
    const value = Number(event.target.value);
    const n = Number.isFinite(value) ? Math.max(LAST_N_MIN, Math.min(LAST_N_MAX, Math.round(value))) : LAST_N_DEFAULT; // clamp LAST_N_MIN..LAST_N_MAX
    onChange({ ...query, lastN: n });
    // Don't auto-run on every keystroke if empty; only when valid number present
    if (!Number.isNaN(n)) {
      onRunQuery();
    }
  };

  onTimestampModeChanged = (value: TimestampMode) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, timestampMode: value });
    onRunQuery();
  };

  onAvroSchemaSourceChanged = (value: AvroSchemaSource) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, avroSchemaSource: value });
    onRunQuery();
  };

  onProtobufSchemaSourceChanged = (value: ProtobufSchemaSource) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, protobufSchemaSource: value });
    onRunQuery();
  };

  onAvroSchemaChanged = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const { onChange, query, onRunQuery } = this.props;
    const newSchema = event.target.value;
    onChange({ ...query, avroSchema: newSchema });
    onRunQuery();

    // Validate the schema after a short delay
    if (this.avroSchemaValidationTimeout) {
      clearTimeout(this.avroSchemaValidationTimeout);
    }
    this.avroSchemaValidationTimeout = setTimeout(() => {
      this.validateAvroSchema(newSchema);
    }, 500);
  };

  onProtobufSchemaChanged = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const { onChange, query, onRunQuery } = this.props;
    const newSchema = event.target.value;
    onChange({ ...query, protobufSchema: newSchema });
    onRunQuery();

    if (this.protobufSchemaValidationTimeout) {
      clearTimeout(this.protobufSchemaValidationTimeout);
    }
    this.protobufSchemaValidationTimeout = setTimeout(() => {
      this.validateProtobufSchema(newSchema);
    }, 500);
  };

  onAvroSchemaFileUpload = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query, onRunQuery } = this.props;
    const file = event.target.files?.[0];
    if (file) {
      // store filename for UI feedback
      this.setState({ selectedAvroFileName: file.name });
      // Validate file type
      if (!file.name.match(/\.(avsc|json)$/i)) {
        this.setState({
          selectedAvroFileName: undefined,
          schemaValidation: { status: 'invalid', message: 'Please upload a .avsc or .json file' },
        });
        return;
      }
      // Validate file size (5MB limit)
      if (file.size > 5 * 1024 * 1024) {
        this.setState({
          selectedAvroFileName: undefined,
          schemaValidation: { status: 'invalid', message: 'File size must be less than 5MB' },
        });
        return;
      }
      const reader = new FileReader();
      reader.onload = (e) => {
        const schema = e.target?.result as string;
        onChange({ ...query, avroSchema: schema });
        onRunQuery();
        // Validate the uploaded schema
        this.validateAvroSchema(schema);
      };
      reader.onerror = () => {
        this.setState({
          selectedAvroFileName: undefined,
          schemaValidation: { status: 'invalid', message: 'Failed to read file' },
        });
      };
      reader.readAsText(file);
    }
  };

  onProtobufSchemaFileUpload = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query, onRunQuery } = this.props;
    const file = event.target.files?.[0];
    if (file) {
      this.setState({ selectedProtobufFileName: file.name });
      if (!file.name.match(/\.(proto)$/i)) {
        this.setState({
          selectedProtobufFileName: undefined,
          protobufSchemaValidation: { status: 'invalid', message: 'Please upload a .proto file' },
        });
        return;
      }
      if (file.size > 5 * 1024 * 1024) {
        this.setState({
          selectedProtobufFileName: undefined,
          protobufSchemaValidation: { status: 'invalid', message: 'File size must be less than 5MB' },
        });
        return;
      }
      const reader = new FileReader();
      reader.onload = (e) => {
        const schema = e.target?.result as string;
        onChange({ ...query, protobufSchema: schema });
        onRunQuery();
        this.validateProtobufSchema(schema);
      };
      reader.onerror = () => {
        this.setState({
          selectedProtobufFileName: undefined,
          protobufSchemaValidation: { status: 'invalid', message: 'Failed to read file' },
        });
      };
      reader.readAsText(file);
    }
  };

  validateAvroSchema = async (schema: string) => {
    if (!schema.trim()) {
      this.setState({
        schemaValidation: { status: 'invalid', message: 'Schema cannot be empty' },
      });
      return;
    }

    this.setState({
      schemaValidation: { status: 'loading' },
    });

    try {
      const result = await this.props.datasource.validateAvroSchema(schema);
      this.setState({
        schemaValidation: {
          status: result.status === 'error' ? 'invalid' : 'valid',
          message: result.message,
        },
      });
    } catch (error) {
      this.setState({
        schemaValidation: {
          status: 'invalid',
          message: 'Failed to validate schema',
        },
      });
    }
  };

  validateSchemaRegistry = async () => {
    this.setState({
      schemaRegistryValidation: { status: 'loading' },
    });

    try {
      const result = await this.props.datasource.validateSchemaRegistry();
      this.setState({
        schemaRegistryValidation: {
          status: result.status === 'error' ? 'invalid' : 'valid',
          message: result.message,
        },
      });
    } catch (error) {
      this.setState({
        schemaRegistryValidation: {
          status: 'invalid',
          message: 'Failed to validate schema registry',
        },
      });
    }
  };

  validateProtobufSchema = async (schema: string) => {
    if (!schema.trim()) {
      this.setState({
        protobufSchemaValidation: { status: 'invalid', message: 'Schema cannot be empty' },
      });
      return;
    }

    this.setState({
      protobufSchemaValidation: { status: 'loading' },
    });

    try {
      const result = await this.props.datasource.validateProtobufSchema(schema);
      this.setState({
        protobufSchemaValidation: {
          status: result.status === 'error' ? 'invalid' : 'valid',
          message: result.message,
        },
      });
    } catch (error) {
      this.setState({
        protobufSchemaValidation: {
          status: 'invalid',
          message: 'Failed to validate schema',
        },
      });
    }
  };

  onMessageFormatChanged = (value: MessageFormat) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, messageFormat: value });
    // Cancel any pending debounced query to prevent race conditions
    this.debouncedRunQuery.cancel();
    // Run query immediately for message format changes
    onRunQuery();
  };

  onKeyFormatChanged = (value: KeyFormat) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, keyFormat: value });
    this.debouncedRunQuery.cancel();
    onRunQuery();
  };

  onTopicSuggestionClick = (topic: string) => {
    const { onChange, query, onRunQuery } = this.props;
    onChange({ ...query, topicName: topic });
    this.setState({ showingSuggestions: false, topicSuggestions: [] });
    this.lastCommittedTopic = topic;
    // Cancel any pending debounced query to prevent race conditions
    this.debouncedRunQuery.cancel();
    onRunQuery();
  };

  onTopicInputBlur = () => {
    // Commit the topic after suggestions close (slight delay so click works)
    if (this.topicBlurTimeout) {
      clearTimeout(this.topicBlurTimeout);
      this.topicBlurTimeout = undefined;
    }
    this.topicBlurTimeout = setTimeout(() => {
      this.setState({ showingSuggestions: false });
      this.commitTopicIfChanged();
      this.topicBlurTimeout = undefined;
    }, 150);
  };

  onTopicInputFocus = () => {
    if (this.state.topicSuggestions.length > 0) {
      this.setState({ showingSuggestions: true });
    }
  };

  onAliasChange = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query } = this.props;
    onChange({ ...query, alias: event.target.value });
    this.debouncedRunQuery();
  };

  onFrequencyHzChange = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query } = this.props;
    const hz = Number(event.target.value);
    // hz <= 0 or invalid → disable throttling (sampleIntervalMs = 0 means "send every message")
    let sampleIntervalMs = 0;
    if (Number.isFinite(hz) && hz > 0) {
      sampleIntervalMs = Math.max(1, Math.round(1000 / hz));
    }
    onChange({ ...query, sampleIntervalMs });
    this.debouncedRunQuery();
  };

  onBufferDurationChange = (event: ChangeEvent<HTMLInputElement>) => {
    const { onChange, query } = this.props;
    const raw = Number(event.target.value);
    const value = Number.isFinite(raw) && raw >= 0 ? Math.floor(raw) : 60;
    onChange({ ...query, bufferDurationSec: value });
    this.debouncedRunQuery();
  };

  onSelectedFieldsChange = (selected: Array<{ label?: string; value?: string }>) => {
    const { onChange, query } = this.props;
    const fields = (selected || [])
      .map((opt) => (opt.value ?? opt.label ?? '').trim())
      .filter((f) => f.length > 0);
    onChange({ ...query, selectedFields: fields });
    this.props.onRunQuery();
  };

  discoverFields = async () => {
    const { datasource, query } = this.props;
    if (!query.topicName) {
      this.setState({ fieldDiscoveryError: 'Enter a topic name first' });
      return;
    }
    this.setState({ discoveringFields: true, fieldDiscoveryError: undefined });
    try {
      const fields = await datasource.discoverFields(
        query.topicName,
        query.messageFormat || MessageFormat.JSON,
        10
      );
      this.setState({ availableFields: fields, discoveringFields: false });
      if (fields.length === 0) {
        this.setState({ fieldDiscoveryError: 'No fields found — topic may be empty or idle' });
      }
    } catch (err: any) {
      console.error('Failed to discover fields:', err);
      this.setState({
        discoveringFields: false,
        fieldDiscoveryError: err?.message || 'Failed to discover fields',
      });
    }
  };

  onAddFilterCondition = () => {
    const { onChange, query, onRunQuery } = this.props;
    const conditions = [...(query.filterConditions || [])];
    conditions.push({ field: '', operator: FilterOperator.EQUALS, value: '' });
    onChange({ ...query, filterConditions: conditions });
    onRunQuery();
  };

  onRemoveFilterCondition = (index: number) => {
    const { onChange, query, onRunQuery } = this.props;
    const conditions = [...(query.filterConditions || [])];
    conditions.splice(index, 1);
    onChange({ ...query, filterConditions: conditions });
    onRunQuery();
  };

  onFilterConditionChange = (index: number, field: keyof FilterCondition, value: string) => {
    const { onChange, query } = this.props;
    const conditions = [...(query.filterConditions || [])];
    conditions[index] = { ...conditions[index], [field]: value };
    onChange({ ...query, filterConditions: conditions });
    this.debouncedRunQuery();
  };

  render() {
    const query = { ...defaultQuery, ...this.props.query };
    const { topicName, partition, autoOffsetReset, timestampMode, lastN, messageFormat } = query;

    return (
      <>
        <InlineFieldRow>
          <InlineField
            label="Topic"
            labelWidth={12}
            tooltip="Kafka topic name + click Fetch to load partitions"
            className={this.props.styles.topicField}
          >
            <div className={this.props.styles.topicContainer}>
              <div className={this.props.styles.topicInputWrapper}>
                <Input
                  id="query-editor-topic"
                  value={topicName || ''}
                  onChange={this.onTopicNameChange}
                  onBlur={this.onTopicInputBlur}
                  onFocus={this.onTopicInputFocus}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') {
                      this.commitTopicIfChanged();
                      this.fetchPartitions();
                    }
                  }}
                  type="text"
                  autoComplete="off"
                  autoCorrect="off"
                  autoCapitalize="none"
                  spellCheck={false}
                  name="gf-topic-search"
                  aria-autocomplete="list"
                  width={24}
                  placeholder="Enter topic name"
                />
                {this.state.showingSuggestions && this.state.topicSuggestions.length > 0 && (
                  <div className={this.props.styles.suggestionDropdown}>
                    {this.state.topicSuggestions.map((topic, index) => (
                      <div
                        key={`${topic}-${index}`}
                        className={this.props.styles.suggestionItem}
                        onClick={() => this.onTopicSuggestionClick(topic)}
                      >
                        {topic}
                      </div>
                    ))}
                  </div>
                )}
              </div>
              <Button
                size="sm"
                variant="secondary"
                onClick={() => {
                  this.commitTopicIfChanged();
                  this.fetchPartitions();
                }}
                disabled={!topicName || this.state.loadingPartitions}
                className={this.props.styles.fetchButton}
              >
                {this.state.loadingPartitions ? <Spinner size={14} /> : 'Fetch'}
              </Button>
              {this.state.partitionSuccess && (
                <div className={this.props.styles.successMessage}>{this.state.partitionSuccess}</div>
              )}
            </div>
          </InlineField>
          <InlineField
            label="Partition"
            labelWidth={13}
            tooltip="Kafka partition selection"
            className={this.props.styles.partitionField}
          >
            <Select
              id="query-editor-partition"
              value={partition}
              options={this.getPartitionOptions()}
              onChange={(opt) => this.onPartitionChange(opt.value as number | 'all')}
              isClearable={false}
              width={22}
              placeholder="Select partition"
              noOptionsMessage="Fetch topic partitions first"
            />
          </InlineField>
        </InlineFieldRow>
        <InlineFieldRow>
          <InlineField
            label="Offset"
            labelWidth={12}
            tooltip="Where to start consuming from for this query"
            className={this.props.styles.offsetField}
          >
            <div className={this.props.styles.offsetContainer}>
              <Select
                width={22}
                value={autoOffsetReset}
                options={autoResetOffsets}
                onChange={(value) => this.onAutoResetOffsetChanged(value.value as AutoOffsetReset)}
              />
              {autoOffsetReset === AutoOffsetReset.LAST_N && (
                <>
                  <InlineLabel width={12}>N</InlineLabel>
                  <Input
                    id="query-editor-last-n"
                    value={String(lastN ?? 100)}
                    type="number"
                    min={1}
                    step={1}
                    width={12}
                    onChange={this.onLastNChanged}
                  />
                </>
              )}
            </div>
          </InlineField>
          <InlineField
            label="Timestamp Mode"
            labelWidth={20}
            tooltip={
              timestampMode === TimestampMode.Message
                ? 'Kafka Event Time: Timestamp from the Kafka message metadata.'
                : 'Dashboard received time: When the Grafana plugin received the message.'
            }
          >
            <Select
              width={25}
              value={timestampMode}
              options={timestampModes}
              onChange={(value) => this.onTimestampModeChanged(value.value as TimestampMode)}
            />
          </InlineField>
        </InlineFieldRow>

        <InlineFieldRow>
          <InlineField
            label="Message Format"
            labelWidth={25}
            tooltip="Format of the Kafka messages (JSON, Avro, or Protobuf)"
          >
            <Select
              width={25}
              value={messageFormat || MessageFormat.JSON}
              options={messageFormats}
              onChange={(value) => this.onMessageFormatChanged(value.value as MessageFormat)}
            />
          </InlineField>
          <InlineField
            label="Alias"
            labelWidth={15}
            tooltip="Custom alias for the query series. Supports placeholders: {{topic}}, {{partition}}, {{refid}}, {{field}}"
          >
            <Input width={25} value={query.alias || ''} onChange={this.onAliasChange} placeholder="Optional alias" />
          </InlineField>
        </InlineFieldRow>

        <InlineFieldRow>
          <InlineField
            label="Key Format"
            labelWidth={25}
            tooltip="Format of Kafka message keys. None = ignore keys (default), String = single 'key' column, JSON = flatten key fields with 'key.' prefix"
          >
            <Select
              width={25}
              value={query.keyFormat || KeyFormat.NONE}
              options={keyFormats}
              onChange={(value) => this.onKeyFormatChanged(value.value as KeyFormat)}
            />
          </InlineField>
        </InlineFieldRow>

        {/* Server-side Field Selection */}
        <InlineFieldRow>
          <InlineField
            label="Fields"
            labelWidth={12}
            tooltip="Fields to include (leave empty for all fields). Click 'Discover' to sample recent messages and populate the dropdown. You can also type custom field names."
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', minWidth: '520px' }}>
              <div style={{ flex: 1, minWidth: '400px' }}>
                <MultiSelect
                  allowCustomValue
                  isClearable
                  closeMenuOnSelect={false}
                  placeholder="All fields (select to filter)"
                  noOptionsMessage="Click Discover to sample fields from the topic"
                  options={(() => {
                    const selected = query.selectedFields || [];
                    const union = Array.from(new Set([...this.state.availableFields, ...selected]));
                    union.sort();
                    return union.map((f) => ({ label: f, value: f }));
                  })()}
                  value={(query.selectedFields || []).map((f) => ({ label: f, value: f }))}
                  onChange={this.onSelectedFieldsChange}
                />
              </div>
              <Button
                size="sm"
                variant="secondary"
                onClick={this.discoverFields}
                disabled={!query.topicName || this.state.discoveringFields}
              >
                {this.state.discoveringFields ? <Spinner size={14} /> : 'Discover'}
              </Button>
              {this.state.fieldDiscoveryError && (
                <InlineLabel color="red">{this.state.fieldDiscoveryError}</InlineLabel>
              )}
              {!this.state.fieldDiscoveryError && this.state.availableFields.length > 0 && (
                <span className={this.props.styles.successMessage}>
                  {this.state.availableFields.length} fields discovered
                </span>
              )}
            </div>
          </InlineField>
        </InlineFieldRow>

        {/* Server-side Filter Conditions */}
        <InlineFieldRow>
          <InlineField
            label="Filters"
            labelWidth={12}
            tooltip="Server-side filters to drop messages that don't match. All conditions must be met (AND logic)."
          >
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
              {(query.filterConditions || []).map((condition, index) => (
                <div key={index} style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                  <div style={{ minWidth: '180px' }}>
                    <Select
                      width={20}
                      allowCustomValue
                      isClearable
                      placeholder="Field name"
                      options={(() => {
                        const known = new Set<string>(this.state.availableFields);
                        if (condition.field) {
                          known.add(condition.field);
                        }
                        const list = Array.from(known).sort();
                        return list.map((f) => ({ label: f, value: f }));
                      })()}
                      value={condition.field ? { label: condition.field, value: condition.field } : null}
                      onChange={(opt) =>
                        this.onFilterConditionChange(index, 'field', (opt?.value as string) || '')
                      }
                    />
                  </div>
                  <Select
                    width={14}
                    value={condition.operator}
                    options={filterOperators}
                    onChange={(opt) => this.onFilterConditionChange(index, 'operator', opt.value as string)}
                  />
                  <Input
                    width={18}
                    value={condition.value}
                    onChange={(e: ChangeEvent<HTMLInputElement>) =>
                      this.onFilterConditionChange(index, 'value', e.target.value)
                    }
                    placeholder="Value"
                  />
                  <Button
                    variant="secondary"
                    size="sm"
                    onClick={() => this.onRemoveFilterCondition(index)}
                    title="Remove filter"
                  >
                    x
                  </Button>
                </div>
              ))}
              <div>
                <Button variant="secondary" size="sm" onClick={this.onAddFilterCondition}>
                  + Add filter
                </Button>
              </div>
            </div>
          </InlineField>
        </InlineFieldRow>

        {/* Sampling and Buffer Controls */}
        <InlineFieldRow>
          <InlineField
            label="Frequency"
            labelWidth={20}
            tooltip="Maximum sample rate per partition, in Hz (server-side throttle). Messages arriving faster than this are dropped. 0 = no throttling (send every message). Default: 10 Hz."
          >
            <Input
              width={18}
              type="number"
              min={0}
              step={1}
              value={(() => {
                const ms = query.sampleIntervalMs ?? 100;
                if (!ms || ms <= 0) {
                  return '0';
                }
                const hz = 1000 / ms;
                // Show integer if close, otherwise 2 decimals
                return Number.isInteger(hz) ? String(hz) : hz.toFixed(2);
              })()}
              onChange={this.onFrequencyHzChange}
              placeholder="10"
              suffix="Hz"
            />
          </InlineField>
          <InlineField
            label="Max delta"
            labelWidth={15}
            tooltip="Rolling time window of points kept in the browser (client-side). Points older than (newest_timestamp - max_delta) are dropped. 0 = no cap. Default: 60s (1 minute)."
          >
            <Input
              width={18}
              type="number"
              min={0}
              step={10}
              value={String(query.bufferDurationSec ?? 60)}
              onChange={this.onBufferDurationChange}
              placeholder="60"
              suffix="s"
            />
          </InlineField>
        </InlineFieldRow>

        {/* Avro Configuration */}
        {messageFormat === MessageFormat.AVRO && (
          <>
            <InlineFieldRow>
              <InlineField
                label="Avro Schema Source"
                labelWidth={25}
                tooltip="Source of the Avro schema for deserialization"
              >
                <Select
                  width={25}
                  value={query.avroSchemaSource || AvroSchemaSource.SCHEMA_REGISTRY}
                  options={avroSchemaSources}
                  onChange={(value) => this.onAvroSchemaSourceChanged(value.value as AvroSchemaSource)}
                />
              </InlineField>
            </InlineFieldRow>

            {query.avroSchemaSource === AvroSchemaSource.SCHEMA_REGISTRY && (
              <InlineFieldRow>
                <InlineField label="Schema Registry" labelWidth={25}>
                  <div className={this.props.styles.testConnectionContainer}>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={this.validateSchemaRegistry}
                      disabled={this.state.schemaRegistryValidation?.status === 'loading'}
                    >
                      {this.state.schemaRegistryValidation?.status === 'loading' ? (
                        <>
                          <Spinner size={12} />
                          Validating...
                        </>
                      ) : (
                        'Test Connection'
                      )}
                    </Button>
                    {this.state.schemaRegistryValidation && (
                      <InlineLabel color={this.state.schemaRegistryValidation.status === 'valid' ? 'green' : 'red'}>
                        {this.state.schemaRegistryValidation.message}
                      </InlineLabel>
                    )}
                  </div>
                </InlineField>
              </InlineFieldRow>
            )}

            {query.avroSchemaSource === AvroSchemaSource.INLINE_SCHEMA && (
              <InlineFieldRow>
                <InlineField
                  label="Avro Schema"
                  labelWidth={20}
                  tooltip="Upload or paste your Avro schema (.avsc file)"
                  className={this.props.styles.schemaField}
                >
                  <div className={this.props.styles.schemaValidationWrapper}>
                    <div className={this.props.styles.fileRow}>
                      <Button variant="secondary" size="sm" onClick={() => this.avroFileInputRef?.current?.click()}>
                        Choose file
                      </Button>
                      <div className={this.props.styles.filenameText}>
                        {this.state.selectedAvroFileName || 'No file selected'}
                      </div>
                      <input
                        ref={this.avroFileInputRef}
                        type="file"
                        accept=".avsc,.json"
                        onChange={this.onAvroSchemaFileUpload}
                        className={this.props.styles.hiddenFileInput}
                      />
                    </div>
                    <textarea
                      value={query.avroSchema || ''}
                      onChange={this.onAvroSchemaChanged}
                      placeholder="Paste your Avro schema JSON here..."
                      rows={8}
                      className={this.props.styles.textArea}
                    />
                    {this.state.schemaValidation && (
                      <div className={this.props.styles.validationMessage}>
                        {this.state.schemaValidation.status === 'loading' ? (
                          <div className={this.props.styles.validationLoading}>
                            <Spinner size={12} />
                            <span className={this.props.styles.validationText}>Validating schema...</span>
                          </div>
                        ) : (
                          <InlineLabel
                            color={this.state.schemaValidation.status === 'valid' ? 'green' : 'red'}
                            className={this.props.styles.validationLabel}
                          >
                            {this.state.schemaValidation.message}
                          </InlineLabel>
                        )}
                      </div>
                    )}
                  </div>
                </InlineField>
              </InlineFieldRow>
            )}
          </>
        )}

        {/* Protobuf Configuration */}
        {messageFormat === MessageFormat.PROTOBUF && (
          <>
            <InlineFieldRow>
              <InlineField
                label="Protobuf Schema Source"
                labelWidth={25}
                tooltip="Source of the Protobuf schema for deserialization"
              >
                <Select
                  width={25}
                  value={query.protobufSchemaSource || ProtobufSchemaSource.SCHEMA_REGISTRY}
                  options={protobufSchemaSources}
                  onChange={(value) => this.onProtobufSchemaSourceChanged(value.value as ProtobufSchemaSource)}
                />
              </InlineField>
            </InlineFieldRow>

            {query.protobufSchemaSource === ProtobufSchemaSource.SCHEMA_REGISTRY && (
              <InlineFieldRow>
                <InlineField label="Schema Registry" labelWidth={25}>
                  <div className={this.props.styles.testConnectionContainer}>
                    <Button
                      variant="secondary"
                      size="sm"
                      onClick={this.validateSchemaRegistry}
                      disabled={this.state.schemaRegistryValidation?.status === 'loading'}
                    >
                      {this.state.schemaRegistryValidation?.status === 'loading' ? (
                        <>
                          <Spinner size={12} />
                          Validating...
                        </>
                      ) : (
                        'Test Connection'
                      )}
                    </Button>
                    {this.state.schemaRegistryValidation && (
                      <InlineLabel color={this.state.schemaRegistryValidation.status === 'valid' ? 'green' : 'red'}>
                        {this.state.schemaRegistryValidation.message}
                      </InlineLabel>
                    )}
                  </div>
                </InlineField>
              </InlineFieldRow>
            )}

            {query.protobufSchemaSource === ProtobufSchemaSource.INLINE_SCHEMA && (
              <InlineFieldRow>
                <InlineField
                  label="Protobuf Schema"
                  labelWidth={20}
                  tooltip="Upload or paste your Protobuf schema (.proto file)"
                  className={this.props.styles.schemaField}
                >
                  <div className={this.props.styles.schemaValidationWrapper}>
                    <div className={this.props.styles.fileRow}>
                      <Button variant="secondary" size="sm" onClick={() => this.protobufFileInputRef?.current?.click()}>
                        Choose file
                      </Button>
                      <div className={this.props.styles.filenameText}>
                        {this.state.selectedProtobufFileName || 'No file selected'}
                      </div>
                      <input
                        ref={this.protobufFileInputRef}
                        type="file"
                        accept=".proto"
                        onChange={this.onProtobufSchemaFileUpload}
                        className={this.props.styles.hiddenFileInput}
                      />
                    </div>
                    <textarea
                      value={query.protobufSchema || ''}
                      onChange={this.onProtobufSchemaChanged}
                      placeholder="Paste your Protobuf schema here..."
                      rows={8}
                      className={this.props.styles.textArea}
                    />
                    {this.state.protobufSchemaValidation && (
                      <div className={this.props.styles.validationMessage}>
                        {this.state.protobufSchemaValidation.status === 'loading' ? (
                          <div className={this.props.styles.validationLoading}>
                            <Spinner size={12} />
                            <span className={this.props.styles.validationText}>Validating schema...</span>
                          </div>
                        ) : (
                          <InlineLabel
                            color={this.state.protobufSchemaValidation.status === 'valid' ? 'green' : 'red'}
                            className={this.props.styles.validationLabel}
                          >
                            {this.state.protobufSchemaValidation.message}
                          </InlineLabel>
                        )}
                      </div>
                    )}
                  </div>
                </InlineField>
              </InlineFieldRow>
            )}
          </>
        )}

        {autoOffsetReset !== AutoOffsetReset.LATEST && (
          <div className={this.props.styles.warningAlert}>
            <Alert severity="warning" title="Potential higher load">
              Starting from Earliest or reading the last N messages can increase load on Kafka and the backend.
            </Alert>
          </div>
        )}
      </>
    );
  }
}

export const QueryEditor = QueryEditorWithStyles;
