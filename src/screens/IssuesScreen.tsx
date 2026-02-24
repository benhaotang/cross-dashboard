import React, { useEffect, useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  ActivityIndicator,
  Linking,
} from 'react-native';
import { useApp } from '../store/AppContext';
import * as gitea from '../services/gitea';
import { GiteaIssue } from '../types';
import AppIcon, { Icons } from '../components/Icon';

type FilterState = 'open' | 'closed' | 'all';

export default function IssuesScreen() {
  const { state, setIssues, setLoading } = useApp();
  const [filter, setFilter] = useState<FilterState>('open');

  useEffect(() => {
    if (state.giteaConfigured && state.issues.length === 0 && state.giteaRepositories.length > 0) {
      loadIssues();
    }
  }, [state.giteaConfigured, state.giteaRepositories]);

  async function loadIssues() {
    if (state.giteaRepositories.length === 0) return;

    setLoading(true);
    try {
      const issues = await gitea.fetchAllIssues(state.giteaRepositories);
      setIssues(issues);
    } catch (error) {
      console.error('Error loading issues:', error);
    } finally {
      setLoading(false);
    }
  }

  function getFilteredIssues(): GiteaIssue[] {
    if (filter === 'all') return state.issues;
    return state.issues.filter((i) => i.state === filter);
  }

  function openIssue(url: string) {
    Linking.openURL(url);
  }

  async function toggleIssueState(issue: GiteaIssue) {
    const [owner, repo] = issue.repository.split('/');
    const newState = issue.state === 'open' ? 'closed' : 'open';

    const success = await gitea.updateIssueState(owner, repo, issue.number, newState);
    if (success) {
      const updated = state.issues.map((i): GiteaIssue =>
        i.id === issue.id ? { ...i, state: newState } : i
      );
      setIssues(updated);
    }
  }

  function renderIssue({ item }: { item: GiteaIssue }) {
    return (
      <TouchableOpacity style={styles.issueCard} onPress={() => openIssue(item.htmlUrl)}>
        <View style={styles.issueHeader}>
          <View style={[styles.stateBadge, item.state === 'open' ? styles.stateOpen : styles.stateClosed]}>
            <Text style={styles.stateText}>{item.state}</Text>
          </View>
          <Text style={styles.issueNumber}>#{item.number}</Text>
        </View>

        <Text style={styles.issueTitle}>{item.title}</Text>
        <Text style={styles.issueRepo}>{item.repository}</Text>

        {item.body && (
          <Text style={styles.issueBody} numberOfLines={2}>
            {item.body}
          </Text>
        )}

        <View style={styles.labelContainer}>
          {item.labels.map((label) => (
            <View key={label.id} style={[styles.label, { backgroundColor: `#${label.color}` }]}>
              <Text style={styles.labelText}>{label.name}</Text>
            </View>
          ))}
        </View>

        <View style={styles.issueFooter}>
          <Text style={styles.issueDate}>
            Updated: {item.updatedAt.toLocaleDateString()}
          </Text>
          <TouchableOpacity
            style={styles.toggleButton}
            onPress={(e) => {
              e.stopPropagation();
              toggleIssueState(item);
            }}
          >
            <Text style={styles.toggleText}>
              {item.state === 'open' ? 'Close' : 'Reopen'}
            </Text>
          </TouchableOpacity>
        </View>
      </TouchableOpacity>
    );
  }

  if (!state.giteaConfigured) {
    return (
      <View style={styles.centered}>
        <Text style={styles.emptyText}>Gitea not configured</Text>
        <Text style={styles.hintText}>Go to Settings to add your Gitea instance</Text>
      </View>
    );
  }

  if (state.giteaRepositories.length === 0) {
    return (
      <View style={styles.centered}>
        <Text style={styles.emptyText}>No repositories configured</Text>
        <Text style={styles.hintText}>Add repositories in Settings</Text>
      </View>
    );
  }

  const filteredIssues = getFilteredIssues();

  return (
    <View style={styles.container}>
      <View style={styles.filterBar}>
        {(['open', 'closed', 'all'] as FilterState[]).map((f) => (
          <TouchableOpacity
            key={f}
            style={[styles.filterButton, filter === f && styles.filterActive]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, filter === f && styles.filterTextActive]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
              {f !== 'all' && ` (${state.issues.filter((i) => i.state === f).length})`}
            </Text>
          </TouchableOpacity>
        ))}
        <TouchableOpacity style={styles.refreshButton} onPress={loadIssues}>
          <AppIcon name={Icons.refresh} size={16} color="#007AFF" />
        </TouchableOpacity>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color="#007AFF" style={styles.loader} />
      ) : (
        <FlatList
          data={filteredIssues}
          renderItem={renderIssue}
          keyExtractor={(item) => item.id.toString()}
          contentContainerStyle={styles.listContent}
          ListEmptyComponent={
            <View style={styles.centered}>
              <Text style={styles.emptyText}>No issues found</Text>
            </View>
          }
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#f5f5f5',
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 20,
  },
  filterBar: {
    flexDirection: 'row',
    padding: 12,
    backgroundColor: '#fff',
    borderBottomWidth: 1,
    borderBottomColor: '#e0e0e0',
  },
  filterButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    marginRight: 8,
    backgroundColor: '#f0f0f0',
  },
  filterActive: {
    backgroundColor: '#007AFF',
  },
  filterText: {
    fontSize: 14,
    color: '#666',
  },
  filterTextActive: {
    color: '#fff',
  },
  refreshButton: {
    marginLeft: 'auto',
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  refreshText: {
    color: '#007AFF',
    fontWeight: '600',
  },
  listContent: {
    padding: 16,
  },
  issueCard: {
    backgroundColor: '#fff',
    padding: 16,
    borderRadius: 8,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  issueHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 8,
  },
  stateBadge: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    marginRight: 8,
  },
  stateOpen: {
    backgroundColor: '#4CAF50',
  },
  stateClosed: {
    backgroundColor: '#9E9E9E',
  },
  stateText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: '600',
  },
  issueNumber: {
    fontSize: 14,
    color: '#999',
  },
  issueTitle: {
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
    marginBottom: 4,
  },
  issueRepo: {
    fontSize: 12,
    color: '#666',
    marginBottom: 8,
  },
  issueBody: {
    fontSize: 14,
    color: '#666',
    marginBottom: 8,
    lineHeight: 20,
  },
  labelContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginBottom: 8,
  },
  label: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    marginRight: 4,
    marginBottom: 4,
  },
  labelText: {
    fontSize: 10,
    color: '#fff',
    fontWeight: '600',
  },
  issueFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 8,
    paddingTop: 8,
    borderTopWidth: 1,
    borderTopColor: '#f0f0f0',
  },
  issueDate: {
    fontSize: 12,
    color: '#999',
  },
  toggleButton: {
    paddingHorizontal: 12,
    paddingVertical: 4,
    borderRadius: 4,
    backgroundColor: '#f0f0f0',
  },
  toggleText: {
    fontSize: 12,
    color: '#666',
  },
  emptyText: {
    fontSize: 16,
    color: '#999',
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
    color: '#bbb',
  },
  loader: {
    marginTop: 40,
  },
});
