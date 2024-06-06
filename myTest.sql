SELECT 
cust.id||'-'||cust.cdmlei COMPANY, 
ES.SENT, NVL(EA.ACPT, 0) ACPT, 
NVL(EN.RJCT, 0) RJCT, 
NVL((ES.SENT - nvl(EA.ACPT,0) - nvl(EN.RJCT,0)), 0) NOREPLY 
FROM 
(SELECT 
counterparty1,  
COUNT((UTI || BIZMSGIDR)) AS SENT 
FROM cdmemirtrdactivity_HIST 
WHERE 
FULFILLSTATUS = 'SENT' 
AND actiontype = 'ValtnUpd' 
    AND REPORTINGTIMESTAMP > '2024-05-02T19:55:00Z'
    AND REPORTINGTIMESTAMP < '2024-05-03T19:55:00Z'
    AND  returnstatus =' ' 
GROUP BY 
counterparty1) ES
LEFT JOIN 
(SELECT 
counterparty1,
COUNT(DISTINCT(UTI || BIZMSGIDR)) 
AS ACPT FROM cdmemirtrdactivity_HIST 
WHERE 
returnstatus = 'ACPT' 
and actiontype = 'ValtnUpd' 
    AND REPORTINGTIMESTAMP > '2024-05-02T19:55:00Z'
    AND REPORTINGTIMESTAMP < '2024-05-03T19:55:00Z' 
    GROUP BY 
    counterparty1) EA
ON  ES.counterparty1 = EA.counterparty1 
LEFT JOIN 
(SELECT 
counterparty1,
COUNT(DISTINCT(UTI || BIZMSGIDR)) AS RJCT 
FROM cdmemirtrdactivity_HIST 
WHERE 
returnstatus = 'RJCT' 
    and actiontype = 'ValtnUpd' 
    AND REPORTINGTIMESTAMP > '2024-05-02T19:55:00Z'
    AND REPORTINGTIMESTAMP < '2024-05-03T19:55:00Z' 
    GROUP BY 
counterparty1) EN 
ON  ES.counterparty1 = EN.counterparty1 
LEFT JOIN (
select 
id,  
cdmlei 
FROM dmcustomer 
WHERE 
audit_current='Y'  
AND audit_entitystate='VER') cust 
on cust.cdmlei = ES.counterparty1
ORDER BY cust.id