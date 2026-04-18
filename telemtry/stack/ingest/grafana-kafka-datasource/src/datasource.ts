import {
  CoreApp,
  DataQueryRequest,
  DataQueryResponse,
  DataSourceInstanceSettings,
  LiveChannelScope,
  ScopedVars,
} from '@grafana/data';
import { DataSourceWithBackend, getGrafanaLiveSrv, getTemplateSrv } from '@grafana/runtime';
import { Observable, merge, throwError, of } from 'rxjs';
import { catchError, startWith } from 'rxjs/operators';
import {
  KafkaDataSourceOptions,
  KafkaQuery,
  AutoOffsetReset,
  defaultQuery,
  MessageFormat,
  AvroSchemaSource,
  ProtobufSchemaSource,
  TimestampMode,
} from './types';

export class DataSource extends DataSourceWithBackend<KafkaQuery, KafkaDataSourceOptions> {
  constructor(instanceSettings: DataSourceInstanceSettings<KafkaDataSourceOptions>) {
    super(instanceSettings);
  }

  getDefaultQuery(_: CoreApp): Partial<KafkaQuery> {
    return {
      topicName: '',
      ...defaultQuery,
    };
  }

  filterQuery(query: KafkaQuery): boolean {
    if (!query?.topicName) {
      return false;
    }
    return query.partition === 'all' || (typeof query.partition === 'number' && query.partition >= 0);
  }

  applyTemplateVariables(query: KafkaQuery, scopedVars: ScopedVars) {
    const templateSrv = getTemplateSrv();
    const topicName = templateSrv.replace(query.topicName, scopedVars);
    let partition: number | 'all' = query.partition;
    // Only apply template replacement if partition is a string that looks like a template variable
    if (typeof partition === 'string' && partition !== 'all') {
      const replaced = templateSrv.replace(partition, scopedVars);
      if (replaced === 'all') {
        partition = 'all';
      } else {
        const parsed = Number.parseInt(replaced, 10);
        partition = Number.isFinite(parsed) && parsed >= 0 ? parsed : 'all';
      }
    } else if (typeof partition === 'number') {
      // Apply template replacement to numeric partitions (might be from saved queries)
      const replaced = templateSrv.replace(String(partition), scopedVars);
      const parsed = Number.parseInt(replaced, 10);
      partition = Number.isFinite(parsed) && parsed >= 0 ? parsed : partition;
    }
    // Sanitize lastN only when mode is LAST_N; otherwise unset it
    let lastN: number | undefined = undefined;
    if (query.autoOffsetReset === AutoOffsetReset.LAST_N) {
      const replacedLastN = templateSrv.replace(String(query.lastN ?? ''), scopedVars);
      const parsed = Number.parseInt(replacedLastN, 10);
      const n = Number.isFinite(parsed) && parsed > 0 ? parsed : 100;
      lastN = n;
    }
    // Sanitize sampleIntervalMs and bufferDurationSec — apply defaults if unset
    const sampleIntervalMs =
      typeof query.sampleIntervalMs === 'number' && query.sampleIntervalMs >= 0
        ? Math.floor(query.sampleIntervalMs)
        : 100; // default 10 Hz
    const bufferDurationSec =
      typeof query.bufferDurationSec === 'number' && query.bufferDurationSec >= 0
        ? Math.floor(query.bufferDurationSec)
        : 60; // default 1 minute
    const result = {
      ...query,
      topicName,
      partition,
      lastN,
      sampleIntervalMs,
      bufferDurationSec,
      // Ensure these fields are preserved with defaults if undefined
      autoOffsetReset: query.autoOffsetReset || AutoOffsetReset.LATEST,
      messageFormat: query.messageFormat || MessageFormat.JSON,
      avroSchemaSource: query.avroSchemaSource || AvroSchemaSource.SCHEMA_REGISTRY,
      protobufSchemaSource: query.protobufSchemaSource || ProtobufSchemaSource.SCHEMA_REGISTRY,
      timestampMode: query.timestampMode || TimestampMode.Message,
    };
    return result;
  }

  query(request: DataQueryRequest<KafkaQuery>): Observable<DataQueryResponse> {
    const observables = request.targets
      .filter((q): q is KafkaQuery => this.filterQuery(q as KafkaQuery))
      .map((q) => {
        const interpolatedQuery = this.applyTemplateVariables(q as KafkaQuery, request.scopedVars);
        // Build path from encoded segments without dangling dashes
        // Include all configuration parameters that should trigger stream restart
        const segments: string[] = [];
        segments.push(encodeURIComponent(String(interpolatedQuery.topicName)));
        segments.push(encodeURIComponent(String(interpolatedQuery.partition)));
        segments.push(encodeURIComponent(String(interpolatedQuery.autoOffsetReset)));
        segments.push(encodeURIComponent(String(interpolatedQuery.messageFormat || 'json')));
        segments.push(encodeURIComponent(String(interpolatedQuery.avroSchemaSource || 'schemaRegistry')));
        segments.push(encodeURIComponent(String(interpolatedQuery.protobufSchemaSource || 'schemaRegistry')));
        // Include key format so changes trigger a new path/subscription
        segments.push(encodeURIComponent(String(interpolatedQuery.keyFormat || 'none')));
        // Include timestamp mode so changes to timestamp handling trigger a new path/subscription
        segments.push(encodeURIComponent(String(interpolatedQuery.timestampMode || 'message')));
        // Include a hash of the Avro schema to detect changes
        const schemaHash = interpolatedQuery.avroSchema
          ? this.generateSchemaHash(interpolatedQuery.avroSchema)
          : 'none';
        segments.push(encodeURIComponent(schemaHash));
        const protobufSchemaHash = interpolatedQuery.protobufSchema
          ? this.generateSchemaHash(interpolatedQuery.protobufSchema)
          : 'none';
        segments.push(encodeURIComponent(protobufSchemaHash));

        if (
          interpolatedQuery.autoOffsetReset === AutoOffsetReset.LAST_N &&
          typeof interpolatedQuery.lastN !== 'undefined'
        ) {
          segments.push(encodeURIComponent(String(interpolatedQuery.lastN)));
        }

        // Include selected fields hash to trigger stream restart when field selection changes
        const selectedFieldsHash =
          interpolatedQuery.selectedFields && interpolatedQuery.selectedFields.length > 0
            ? this.generateSchemaHash(interpolatedQuery.selectedFields.join(','))
            : 'all';
        segments.push(encodeURIComponent(selectedFieldsHash));

        // Include filter conditions hash to trigger stream restart when filters change
        const filtersHash =
          interpolatedQuery.filterConditions && interpolatedQuery.filterConditions.length > 0
            ? this.generateSchemaHash(JSON.stringify(interpolatedQuery.filterConditions))
            : 'nofilter';
        segments.push(encodeURIComponent(filtersHash));

        // Include sample interval so changes trigger a new stream subscription
        segments.push(encodeURIComponent(String(interpolatedQuery.sampleIntervalMs ?? 0)));

        // Include buffer duration so changes resize the Grafana Live buffer
        // (without this, shrinking maxDelta only hides points; they stay in memory)
        segments.push(encodeURIComponent(String(interpolatedQuery.bufferDurationSec ?? 0)));

        // Include RefID to ensure separate streams for different queries in the same panel
        segments.push(encodeURIComponent(String(interpolatedQuery.refId ?? 'no-refid')));
        // Include Alias to trigger stream restart when alias changes
        // Use a slug + hash strategy to ensure safe path characters while maintaining readability
        const alias = interpolatedQuery.alias || '';
        if (alias) {
          const slug = alias.replace(/[^a-zA-Z0-9_]/g, '').slice(0, 64);
          const hash = this.generateSchemaHash(alias);
          segments.push(slug ? `${slug}-${hash}` : hash);
        } else {
          segments.push('no-alias');
        }

        const path = segments.join('-');

        // Build buffer options for Grafana Live; bufferDurationSec caps the rolling time window
        // via maxDelta (seconds). Also set maxLength as a safety cap computed from sample rate.
        const streamRequest: any = {
          addr: {
            scope: LiveChannelScope.DataSource,
            namespace: this.uid,
            // Grafana 12.4+ expects `stream`, while older versions use `namespace`
            stream: this.uid,
            path,
            data: interpolatedQuery,
          },
        };
        if (typeof interpolatedQuery.bufferDurationSec === 'number' && interpolatedQuery.bufferDurationSec > 0) {
          // Grafana Live's maxDelta is compared against the numeric `time` field values.
          // Bridge emits `time` as Unix ms, so multiply the user-entered seconds by 1000.
          const maxDelta = interpolatedQuery.bufferDurationSec * 1000;
          // Safety cap on point count: 2x the expected number of points at the configured
          // sample rate over the buffer window, with a hard floor/ceiling.
          const interval = interpolatedQuery.sampleIntervalMs && interpolatedQuery.sampleIntervalMs > 0
            ? interpolatedQuery.sampleIntervalMs
            : 100;
          const expectedPoints = Math.ceil((maxDelta * 1000) / interval);
          const maxLength = Math.min(100000, Math.max(100, expectedPoints * 2));
          streamRequest.buffer = { maxDelta, maxLength };
        }
        return getGrafanaLiveSrv()
          .getDataStream(streamRequest)
          .pipe(
            startWith({ data: [] }),
            catchError((err) => {
              console.error('Stream error for path:', path, 'error:', err);
              return throwError(() => ({
                message: `Error connecting to Kafka topic ${interpolatedQuery.topicName}: ${err.message}`,
                status: 'error',
              }));
            })
          );
      });

    return observables.length ? merge(...observables) : of({ data: [] });
  }

  async getTopicPartitions(topicName: string): Promise<number[]> {
    try {
      const response = await this.getResource('partitions', { topic: topicName });
      return response.partitions || [];
    } catch (err: any) {
      // Re-throw to let Grafana surface the error toast, preserving 404 messages
      throw err;
    }
  }

  async searchTopics(prefix: string, limit = 5): Promise<string[]> {
    const response = await this.getResource('topics', { prefix, limit: String(limit) });
    return response.topics || [];
  }

  /**
   * Sample recent messages from a topic and return the list of flattened field names.
   * Used to populate the "Fields" multi-select dropdown in the query editor.
   */
  async discoverFields(topic: string, messageFormat: string, limit = 5): Promise<string[]> {
    const response = await this.getResource('discover-fields', {
      topic,
      messageFormat,
      limit: String(limit),
    });
    return response.fields || [];
  }

  async validateSchemaRegistry(): Promise<{ status: string; message: string }> {
    try {
      const response = (await this.getResource('validate-schema-registry')) as any;
      return {
        status: response.status || 'ok',
        message: response.message || 'Schema registry is accessible',
      };
    } catch (err: any) {
      return {
        status: 'error',
        message: err?.message || 'Failed to validate schema registry',
      };
    }
  }

  async validateAvroSchema(schema: string): Promise<{ status: string; message: string }> {
    try {
      const response = (await this.postResource('validate-avro-schema', { schema })) as any;
      return {
        status: response.status || 'ok',
        message: response.message || 'Schema is valid',
      };
    } catch (err: any) {
      return {
        status: 'error',
        message: err?.message || 'Failed to validate schema',
      };
    }
  }

  async validateProtobufSchema(schema: string): Promise<{ status: string; message: string }> {
    try {
      const response = (await this.postResource('validate-protobuf-schema', { schema })) as any;
      return {
        status: response.status || 'ok',
        message: response.message || 'Schema is valid',
      };
    } catch (err: any) {
      return {
        status: 'error',
        message: err?.message || 'Failed to validate schema',
      };
    }
  }

  /**
   * Generate a stable hash for schema content to detect changes
   * Uses a simple but effective hash function for schema comparison
   */
  private generateSchemaHash(schema: string): string {
    let hash = 0;
    for (let i = 0; i < schema.length; i++) {
      const char = schema.charCodeAt(i);
      hash = (hash << 5) - hash + char;
      hash = hash & hash; // Convert to 32-bit integer
    }
    // Return absolute value as base36 string for shorter, URL-safe hash
    return Math.abs(hash).toString(36);
  }
}
