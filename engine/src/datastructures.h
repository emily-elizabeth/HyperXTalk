template <typename T>
struct MCLinkedListNode
{
private:
	MCLinkedListNode *m_next;
	T m_data;
public:
	MCLinkedListNode(T p_data = NULL)
	{
		m_next = NULL;
		m_data = p_data;
	}

	void clear()
	{
		if (m_next != NULL)
		{
			m_next->clear();
			delete m_next;
			m_next = NULL;
		}
	}

	MCLinkedListNode *next()
	{
		return m_next;
	}

	T data()
	{
		return m_data;
	}

	void append(T p_data)
	{
		if (m_next == NULL)
			m_next = new MCLinkedListNode(p_data);
		else
			m_next->append(p_data);
	}
};

template<typename T>
struct MCLinkedList
{
private:
	MCLinkedListNode<T> *m_first;
	uint4 m_length;
public:
	MCLinkedList<T>()
	{
		m_first = NULL;
		m_length = 0;
	}

	~MCLinkedList<T>()
	{
		clear();
	}

	uint4 length()
	{
		return m_length;
	}

	MCLinkedListNode<T> *first()
	{
		return m_first;
	}

	void append(T p_data)
	{
		if (m_first == NULL)
			m_first = new MCLinkedListNode<T>(p_data);
		else
			m_first->append(p_data);
	}

	void clear()
	{
		if (m_first != NULL)
		{
			m_first->clear();
			delete m_first;
			m_first = NULL;
		}
	}
};
